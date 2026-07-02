#include "WIFI_MQTT.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <ctype.h>
#include <stdlib.h>
#include <time.h>

WifiMqttManager *WifiMqttManager::s_instance = nullptr;

static const char *NET_NVS_NAMESPACE = "chatnet";
static const char *NVS_MSG_COUNTER_KEY = "msgCounter";
static const char *NVS_LAST_SEQ_KEY = "lastSeq";
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;
static const uint32_t HTTP_TIMEOUT_MS = 2500;
static const uint16_t HISTORY_PAGE_LIMIT = 20;

static uint32_t normalizeEpochSeconds(uint64_t raw)
{
    if (raw == 0) return 0;

    // 兼容毫秒时间戳。当前 Unix 秒约 17 亿，毫秒约 1.7 万亿。
    if (raw > 20000000000ULL) {
        raw /= 1000ULL;
    }

    if (raw > 0xFFFFFFFFULL) {
        return 0;
    }

    return (uint32_t)raw;
}

static bool parseDateNumbers(const char *text, int values[6])
{
    if (!text || !values) return false;

    uint8_t count = 0;
    const char *p = text;
    while (*p && count < 6) {
        while (*p && !isdigit((unsigned char)*p)) {
            p++;
        }

        if (!*p) break;

        values[count++] = atoi(p);

        while (*p && isdigit((unsigned char)*p)) {
            p++;
        }
    }

    return count >= 5;
}

static uint32_t parseTimestampString(const char *text)
{
    if (!text || text[0] == '\0') return 0;

    bool allDigits = true;
    for (const char *p = text; *p; p++) {
        if (!isdigit((unsigned char)*p)) {
            allDigits = false;
            break;
        }
    }

    if (allDigits) {
        return normalizeEpochSeconds(strtoull(text, nullptr, 10));
    }

    int v[6] = {0, 0, 0, 0, 0, 0};
    if (!parseDateNumbers(text, v)) return 0;

    if (v[0] < 2000 || v[1] < 1 || v[1] > 12 || v[2] < 1 || v[2] > 31) {
        return 0;
    }

    struct tm tmv;
    memset(&tmv, 0, sizeof(tmv));
    tmv.tm_year = v[0] - 1900;
    tmv.tm_mon = v[1] - 1;
    tmv.tm_mday = v[2];
    tmv.tm_hour = v[3];
    tmv.tm_min = v[4];
    tmv.tm_sec = v[5];
    tmv.tm_isdst = -1;

    time_t epoch = mktime(&tmv);
    if (epoch <= 0) return 0;
    return (uint32_t)epoch;
}

static uint32_t readTimestamp(JsonVariantConst value)
{
    if (value.isNull()) return 0;

    if (value.is<const char*>()) {
        return parseTimestampString(value.as<const char*>());
    }

    return normalizeEpochSeconds(value.as<uint64_t>());
}

static bool httpGetJson(const String& url, JsonDocument& doc, const char *tag)
{
    Serial.print("[HTTP] GET ");
    Serial.println(url);

    HTTPClient http;
    http.setTimeout(HTTP_TIMEOUT_MS);

    if (!http.begin(url)) {
        Serial.print("[HTTP] ");
        Serial.print(tag);
        Serial.println(" begin failed");
        return false;
    }

    int code = http.GET();
    Serial.print("[HTTP] ");
    Serial.print(tag);
    Serial.print(" code=");
    Serial.println(code);

    if (code != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();

    if (err) {
        Serial.print("[HTTP] ");
        Serial.print(tag);
        Serial.print(" parse error: ");
        Serial.println(err.c_str());
        return false;
    }

    return true;
}

static bool readChatMessageFromJson(JsonObjectConst obj, ChatMessage& msg)
{
    if (obj.isNull()) return false;

    const char *type = obj["type"] | "";
    if (strcmp(type, "chat") != 0) {
        return false;
    }

    msg.seq = obj["seq"] | 0;
    msg.room = obj["room"] | "";
    msg.clientMsgId = obj["client_msg_id"] | "";
    msg.from = obj["from"] | "";
    msg.text = obj["text"] | "";
    msg.ts = readTimestamp(obj["ts"]);
    msg.text.trim();

    return msg.seq > 0 && msg.text.length() > 0;
}

WifiMqttManager::WifiMqttManager()
    : _mqtt(_wifiClient)
{
    s_instance = this;
}

void WifiMqttManager::begin(const char *ssid,
                            const char *pass,
                            const char *host,
                            uint16_t port,
                            const char *topicTx,
                            const char *topicRx,
                            const char *cmdTopic,
                            const char *deviceId,
                            const char *roomId,
                            const char *httpServerBase)
{
    _ssid    = ssid ? ssid : "";
    _pass    = pass ? pass : "";
    _host    = host ? host : "";
    _port    = port;
    _topicTx = topicTx ? topicTx : "";
    _topicRx = topicRx ? topicRx : "";
    _cmdTopic = cmdTopic ? cmdTopic : "";

    _deviceId = (deviceId && deviceId[0] != '\0') ? deviceId : DEVICE_ID;
    _roomId = (roomId && roomId[0] != '\0') ? roomId : ROOM_ID;
    _httpServerBase = (httpServerBase && httpServerBase[0] != '\0') ? httpServerBase : HTTP_SERVER_BASE;
    initChipSuffix();
    loadPersistentState();

    _mqtt.setServer(_host.c_str(), _port);
    _mqtt.setCallback(WifiMqttManager::mqttCallbackRouter);

    // PubSubClient 默认包长偏小。聊天 JSON 里可能包含中文 UTF-8，
    // 提前把 MQTT 缓冲区放大，避免稍长消息被截断或 publish 失败。
    _mqtt.setBufferSize(2048);
}

void WifiMqttManager::startWifi()
{
    if (_ssid.length() == 0) {
        Serial.println("[WIFI] failed");
        return;
    }

    Serial.println("[WIFI] connecting...");
    _wifiConnectStartMs = millis();
    _wifiFailureLogged = false;

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(_ssid.c_str(), _pass.c_str());
}

void WifiMqttManager::restartWifi()
{
    disconnectMqtt();
    WiFi.disconnect();
    startWifi();
}

bool WifiMqttManager::connectMqtt()
{
    if (!wifiConnected()) return false;
    if (_mqtt.connected()) return true;

    Serial.println("[MQTT] connecting...");

    String clientId = _deviceId;
    clientId += "-";
    clientId += _chipSuffix;

    bool ok = _mqtt.connect(clientId.c_str());
    if (!ok) {
        Serial.print("[MQTT] connect failed, state=");
        Serial.println(_mqtt.state());
        if (_lastMqttConnected) {
            _lastMqttConnected = false;
            emitMqttState(false);
        }
        return false;
    }

    Serial.println("[MQTT] connected");

    if (!_mqtt.subscribe(_topicRx.c_str())) {
        Serial.print("[MQTT] subscribe failed: ");
        Serial.println(_topicRx);
        _mqtt.disconnect();
        if (_lastMqttConnected) {
            _lastMqttConnected = false;
            emitMqttState(false);
        }
        return false;
    }

    Serial.print("[MQTT] subscribed: ");
    Serial.println(_topicRx);

    // 控制 topic 只负责 OTA/配置命令。订阅失败不会影响聊天主链路。
    if (_cmdTopic.length() > 0) {
        if (_mqtt.subscribe(_cmdTopic.c_str())) {
            Serial.print("[MQTT] command subscribed: ");
            Serial.println(_cmdTopic);
        } else {
            Serial.print("[MQTT] command subscribe failed: ");
            Serial.println(_cmdTopic);
        }
    }

    // 这里不要再往 up topic 发布 “ESP32 connected” 之类上线消息。
    // 新服务器把 up topic 当作聊天入口，只接受 room/from/client_msg_id/text JSON。
    _lastMqttConnected = true;
    emitMqttState(true);
    return true;
}

void WifiMqttManager::disconnectMqtt()
{
    if (_mqtt.connected()) {
        _mqtt.disconnect();
    }

    if (_lastMqttConnected) {
        _lastMqttConnected = false;
        emitMqttState(false);
    }
}

bool WifiMqttManager::publish(const char *payload)
{
    if (payload == nullptr || payload[0] == '\0') return false;
    if (!_mqtt.connected()) {
        Serial.println("[MQTT] publish failed");
        return false;
    }

    bool ok = _mqtt.publish(_topicTx.c_str(), payload);
    if (!ok) {
        Serial.println("[MQTT] publish failed");
    }
    return ok;
}

bool WifiMqttManager::publishChat(const String& text)
{
    String cleanText = text;
    cleanText.trim();
    if (cleanText.length() == 0) return false;

    if (!_mqtt.connected()) {
        // MQTT 未连接时不能清空输入框。main.cpp 会根据 false 保留 ime.text()。
        Serial.println("[MQTT] publish failed");
        return false;
    }

    /*
       ESP32 上行只发送 room/from/client_msg_id/text，不自己生成 seq。
       seq 由服务器保存 SQLite 后统一分配，再通过 down topic 返回。

       注意：publish 成功只代表 JSON 已经交给 MQTT Broker，
       不代表 Node.js 服务器已经保存成功。因此 UI 最终显示必须以
       服务器从 chat/room001/down 返回的标准消息为准。
    */
    String clientMsgId = nextClientMsgId();

    JsonDocument doc;
    doc["room"] = _roomId;
    doc["from"] = _deviceId;
    doc["client_msg_id"] = clientMsgId;
    doc["text"] = cleanText;

    String payload;
    serializeJson(doc, payload);

    Serial.print("[MQTT TX] ");
    Serial.println(payload);

    bool ok = _mqtt.publish(_topicTx.c_str(), payload.c_str());
    if (!ok) {
        Serial.println("[MQTT] publish failed");
    }
    return ok;
}

bool WifiMqttManager::syncHistory(uint32_t sinceSeq, uint16_t limit)
{
    /*
       开机联网后通过 /api/messages 拉取历史消息。
       since 表示本机已经处理到的最后 seq，limit 第一阶段固定 100。

       HTTP 返回的 messages 数组和 MQTT down topic 的单条消息格式一致，
       所以每条历史消息都会进入 handleChatMessage()。这样历史消息和实时
       MQTT 消息共用同一套 type/room/text/seq 检查、左右气泡判断和去重逻辑。

       如果 HTTP 同步失败，函数直接返回 false，不阻塞小手机继续连接 MQTT。
    */
    if (!wifiConnected()) {
        Serial.println("[HTTP] sync skipped, WiFi not connected");
        return false;
    }

    if (limit == 0) limit = 100;

    Serial.print("[HTTP] sync since=");
    Serial.println(sinceSeq);

    String url = _httpServerBase;
    url += "/api/messages?room=";
    url += _roomId;
    url += "&since=";
    url += String(sinceSeq);
    url += "&limit=";
    url += String(limit);

    JsonDocument doc;
    if (!httpGetJson(url, doc, "sync")) {
        return false;
    }

    JsonArrayConst messages = doc["messages"].as<JsonArrayConst>();
    if (messages.isNull()) {
        Serial.println("[HTTP] sync count=0");
        return true;
    }

    Serial.print("[HTTP] sync count=");
    Serial.println(messages.size());

    for (JsonObjectConst item : messages) {
        ChatMessage msg;
        if (!readChatMessageFromJson(item, msg)) {
            continue;
        }

        Serial.print("[HTTP] msg seq=");
        Serial.print(msg.seq);
        Serial.print(" from=");
        Serial.print(msg.from);
        Serial.print(" text=");
        Serial.println(msg.text);

        handleChatMessage(msg, true);
    }

    return true;
}

bool WifiMqttManager::syncLatestHistory(uint16_t limit)
{
    /*
       重建“服务器最新 N 条”聊天窗口。
       先用 /health 拿 latestSeq，再按小页请求 since 历史，避免一次解析 100 条
       大 JSON 导致堆内存紧张或主循环长时间卡住。
    */
    if (!wifiConnected()) {
        Serial.println("[HTTP] latest skipped, WiFi not connected");
        return false;
    }

    if (limit == 0 || limit > 100) limit = 100;

    Serial.print("[HTTP] latest limit=");
    Serial.println(limit);

    String healthUrl = _httpServerBase;
    healthUrl += "/health";

    JsonDocument healthDoc;
    if (!httpGetJson(healthUrl, healthDoc, "latest meta")) {
        return false;
    }

    const uint32_t latestSeq = healthDoc["latestSeq"] | 0;
    if (latestSeq == 0) {
        Serial.println("[HTTP] latest empty");
        return true;
    }

    const uint32_t startSeq = (latestSeq > limit) ? (latestSeq - limit) : 0;

    Serial.print("[HTTP] latestSeq=");
    Serial.print(latestSeq);
    Serial.print(" startSeq=");
    Serial.println(startSeq);

    const uint32_t oldLastSeq = _lastLocalSeq;
    _lastLocalSeq = startSeq;
    _historyRebuildMode = true;

    bool ok = true;
    uint16_t applied = 0;
    uint32_t firstSeq = 0;
    uint32_t lastSeq = 0;
    uint32_t cursor = startSeq;

    while (cursor < latestSeq && applied < limit) {
        uint16_t pageLimit = limit - applied;
        if (pageLimit > HISTORY_PAGE_LIMIT) pageLimit = HISTORY_PAGE_LIMIT;

        String url = _httpServerBase;
        url += "/api/messages?room=";
        url += _roomId;
        url += "&since=";
        url += String(cursor);
        url += "&limit=";
        url += String(pageLimit);

        JsonDocument doc;
        if (!httpGetJson(url, doc, "latest page")) {
            ok = false;
            break;
        }

        JsonArrayConst messages = doc["messages"].as<JsonArrayConst>();
        if (messages.isNull()) {
            Serial.println("[HTTP] latest page missing messages");
            ok = false;
            break;
        }

        if (messages.size() == 0) {
            break;
        }

        uint32_t oldCursor = cursor;
        for (JsonObjectConst item : messages) {
            ChatMessage msg;
            if (!readChatMessageFromJson(item, msg)) {
                continue;
            }

            if (handleChatMessage(msg, true)) {
                if (firstSeq == 0) firstSeq = msg.seq;
                lastSeq = msg.seq;
                applied++;
            }

            if (msg.seq > cursor) {
                cursor = msg.seq;
            }

            if (applied >= limit || cursor >= latestSeq) {
                break;
            }
        }

        if (cursor == oldCursor) break;
        yield();
    }

    _historyRebuildMode = false;

    if (ok && applied > 0) {
        saveLastLocalSeq();
    } else {
        _lastLocalSeq = oldLastSeq;
    }

    Serial.print("[HTTP] latest applied=");
    Serial.print(applied);
    Serial.print(" firstSeq=");
    Serial.print(firstSeq);
    Serial.print(" lastSeq=");
    Serial.println(lastSeq);

    return ok;
}

void WifiMqttManager::update()
{
    bool nowWifi = wifiConnected();
    if (nowWifi != _lastWifiConnected) {
        _lastWifiConnected = nowWifi;

        if (nowWifi) {
            Serial.print("[WIFI] connected, IP=");
            Serial.println(WiFi.localIP());
            _wifiConnectStartMs = 0;
            _wifiFailureLogged = false;
        } else {
            Serial.println("[WIFI] failed");
            disconnectMqtt();
        }

        emitWifiState(nowWifi);
    }

    if (!nowWifi &&
        _wifiConnectStartMs != 0 &&
        !_wifiFailureLogged &&
        (millis() - _wifiConnectStartMs) >= WIFI_CONNECT_TIMEOUT_MS) {
        Serial.println("[WIFI] failed");
        _wifiFailureLogged = true;
    }

    bool nowMqtt = mqttConnected();
    if (nowMqtt != _lastMqttConnected) {
        _lastMqttConnected = nowMqtt;
        emitMqttState(nowMqtt);
    }

    if (nowMqtt) {
        _mqtt.loop();
    }
}

bool WifiMqttManager::wifiConnected() const
{
    return WiFi.status() == WL_CONNECTED;
}

bool WifiMqttManager::mqttConnected()
{
    return _mqtt.connected();
}

IPAddress WifiMqttManager::localIP() const
{
    return WiFi.localIP();
}

void WifiMqttManager::onMessage(MessageCallback cb)
{
    _msgCb = cb;
}

void WifiMqttManager::setChatMessageCallback(ChatMessageCallback cb)
{
    _chatCb = cb;
}

void WifiMqttManager::setCommandCallback(CommandCallback cb)
{
    _cmdCb = cb;
}

void WifiMqttManager::onWifiStateChange(StateCallback cb)
{
    _wifiCb = cb;
}

void WifiMqttManager::onMqttStateChange(StateCallback cb)
{
    _mqttCb = cb;
}

void WifiMqttManager::emitWifiState(bool connected)
{
    if (_wifiCb) _wifiCb(connected);
}

void WifiMqttManager::emitMqttState(bool connected)
{
    if (_mqttCb) _mqttCb(connected);
}

void WifiMqttManager::mqttCallbackRouter(char *topic, byte *payload, unsigned int length)
{
    if (s_instance) {
        s_instance->handleMqttMessage(topic, payload, length);
    }
}

void WifiMqttManager::handleMqttMessage(char *topic, byte *payload, unsigned int length)
{
    String msg;
    msg.reserve(length);

    for (unsigned int i = 0; i < length; i++) {
        msg += (char)payload[i];
    }

    Serial.print("[MQTT RX] ");
    Serial.print(topic);
    Serial.print(" => ");
    Serial.println(msg);

    if (_msgCb) {
        _msgCb(topic, msg);
    }

    if (_cmdTopic.length() > 0 && strcmp(topic, _cmdTopic.c_str()) == 0) {
        if (_cmdCb) {
            _cmdCb(msg);
        }
        return;
    }

    if (_topicRx.length() == 0 || strcmp(topic, _topicRx.c_str()) != 0) {
        return;
    }

    parseAndHandleChatJson(msg, false);
}

bool WifiMqttManager::parseAndHandleChatJson(const String& payload, bool fromHistory)
{
    /*
       MQTT 收到的是服务器 down topic 的标准聊天 JSON。
       这里先做协议层解析和校验，只生成 ChatMessage，不直接操作 LVGL。
       UI 显示交给 main.cpp 的回调处理，这样网络层不会和输入法、按键、
       小手机界面绑死在一起。
    */
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.print("[MQTT] parse error: ");
        Serial.println(err.c_str());
        return false;
    }

    ChatMessage msg;
    if (!readChatMessageFromJson(doc.as<JsonObjectConst>(), msg)) {
        return false;
    }

    return handleChatMessage(msg, fromHistory);
}

bool WifiMqttManager::handleChatMessage(const ChatMessage& msg, bool fromHistory)
{
    /*
       第一阶段使用 lastLocalSeq 做基础去重。
       历史同步和实时 MQTT 可能有重叠，例如 HTTP 刚拉到 seq=10，
       MQTT 又收到同一条 seq=10。此时 seq <= lastLocalSeq 就说明
       这条消息大概率已经显示过，应该忽略，避免聊天区重复气泡。

       后续如果要更严谨，可以再维护最近若干个 client_msg_id 缓存，
       用来处理极端情况下的乱序或服务器重放。
    */
    const char *source = fromHistory ? "[HTTP]" : "[MQTT]";

    if (msg.room != _roomId) {
        return false;
    }

    if (msg.text.length() == 0) {
        return false;
    }

    if (msg.seq == 0) {
        return false;
    }

    if (!_historyRebuildMode && msg.seq <= _lastLocalSeq) {
        Serial.print(source);
        Serial.print(" duplicate seq=");
        Serial.print(msg.seq);
        Serial.print(" last=");
        Serial.println(_lastLocalSeq);
        return false;
    }

    if (_chatCb) {
        _chatCb(msg);
    }

    if (msg.seq > _lastLocalSeq) {
        _lastLocalSeq = msg.seq;
        if (!_historyRebuildMode) {
            saveLastLocalSeq();
        }
    }

    return true;
}

String WifiMqttManager::nextClientMsgId()
{
    /*
       client_msg_id 是“每一条消息”的唯一 ID，不是设备固定 ID。

       不能只用 millis()：ESP32 重启后 millis() 会从 0 重新开始，
       如果设备短时间内重启再发送，ID 就可能和旧消息撞车。

       当前规则：
       device_id + "_" + ESP32 芯片 ID 后 8 位 HEX + "_" + NVS 递增计数

       msgCounter 保存在 Preferences/NVS 中。每次发送前先 ++，并立刻写回 NVS，
       这样即使 ESP32 断电重启，下一条消息也会继续递增，不会回到 1。
       服务器可以根据 client_msg_id 防止重复保存同一条消息。
    */
    if (!_stateLoaded) {
        loadPersistentState();
    }

    _msgCounter++;

    Preferences prefs;
    if (prefs.begin(NET_NVS_NAMESPACE, false)) {
        prefs.putUInt(NVS_MSG_COUNTER_KEY, _msgCounter);
        prefs.end();
    } else {
        Serial.println("[NVS] msgCounter save failed");
    }

    String id = _deviceId;
    id += "_";
    id += _chipSuffix;
    id += "_";
    id += String(_msgCounter);
    return id;
}

void WifiMqttManager::loadPersistentState()
{
    Preferences prefs;
    if (prefs.begin(NET_NVS_NAMESPACE, true)) {
        _msgCounter = prefs.getUInt(NVS_MSG_COUNTER_KEY, 0);
        _lastLocalSeq = prefs.getUInt(NVS_LAST_SEQ_KEY, 0);
        prefs.end();
    }

    _stateLoaded = true;
}

//本机保存服务器聊天室的消息seq
void WifiMqttManager::saveLastLocalSeq()
{
    Preferences prefs;
    if (prefs.begin(NET_NVS_NAMESPACE, false)) {
        prefs.putUInt(NVS_LAST_SEQ_KEY, _lastLocalSeq);
        prefs.end();
    }
}

void WifiMqttManager::initChipSuffix()
{
    uint64_t mac = ESP.getEfuseMac();
    uint32_t low = static_cast<uint32_t>(mac & 0xFFFFFFFFULL);

    char buf[9] = {0};
    snprintf(buf, sizeof(buf), "%08lX", static_cast<unsigned long>(low));
    _chipSuffix = buf;
}
