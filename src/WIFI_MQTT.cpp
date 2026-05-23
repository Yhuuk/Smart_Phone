#include "WIFI_MQTT.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>

WifiMqttManager *WifiMqttManager::s_instance = nullptr;

static const char *NET_NVS_NAMESPACE = "chatnet";
static const char *NVS_MSG_COUNTER_KEY = "msgCounter";
static const char *NVS_LAST_SEQ_KEY = "lastSeq";
static const uint32_t WIFI_CONNECT_TIMEOUT_MS = 15000;

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
    msg.ts = obj["ts"] | 0;
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
                            const char *topicRx)
{
    _ssid    = ssid;
    _pass    = pass;
    _host    = host;
    _port    = port;
    _topicTx = topicTx;
    _topicRx = topicRx;

    _deviceId = DEVICE_ID;
    initChipSuffix();
    loadPersistentState();

    _mqtt.setServer(_host, _port);
    _mqtt.setCallback(WifiMqttManager::mqttCallbackRouter);

    // PubSubClient 默认包长偏小。聊天 JSON 里可能包含中文 UTF-8，
    // 提前把 MQTT 缓冲区放大，避免稍长消息被截断或 publish 失败。
    _mqtt.setBufferSize(1024);
}

void WifiMqttManager::startWifi()
{
    if (_ssid == nullptr || _ssid[0] == '\0') {
        Serial.println("[WIFI] failed");
        return;
    }

    Serial.println("[WIFI] connecting...");
    _wifiConnectStartMs = millis();
    _wifiFailureLogged = false;

    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.begin(_ssid, _pass);
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

    if (!_mqtt.subscribe(_topicRx)) {
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

    bool ok = _mqtt.publish(_topicTx, payload);
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
    doc["room"] = ROOM_ID;
    doc["from"] = _deviceId;
    doc["client_msg_id"] = clientMsgId;
    doc["text"] = cleanText;

    String payload;
    serializeJson(doc, payload);

    Serial.print("[MQTT TX] ");
    Serial.println(payload);

    bool ok = _mqtt.publish(_topicTx, payload.c_str());
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

    String url = String(HTTP_SERVER_BASE);
    url += "/api/messages?room=";
    url += ROOM_ID;
    url += "&since=";
    url += String(sinceSeq);
    url += "&limit=";
    url += String(limit);

    Serial.print("[HTTP] GET ");
    Serial.println(url);

    HTTPClient http;
    http.setTimeout(5000);

    if (!http.begin(url)) {
        Serial.println("[HTTP] begin failed");
        return false;
    }

    int code = http.GET();
    Serial.print("[HTTP] code=");
    Serial.println(code);

    if (code != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.print("[HTTP] parse error: ");
        Serial.println(err.c_str());
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

    if (_topicRx == nullptr || strcmp(topic, _topicRx) != 0) {
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

    if (msg.room != ROOM_ID) {
        return false;
    }

    if (msg.text.length() == 0) {
        return false;
    }

    if (msg.seq == 0) {
        return false;
    }

    if (msg.seq <= _lastLocalSeq) {
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
        saveLastLocalSeq();
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
       DEVICE_ID + "_" + ESP32 芯片 ID 后 8 位 HEX + "_" + NVS 递增计数

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
