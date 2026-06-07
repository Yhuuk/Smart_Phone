#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

// =====================
// 网络与服务器配置
// =====================
// WiFi 和 MQTT Broker 需要和你的 Node.js 服务器环境保持一致。
// MQTT_HOST 继续使用服务器 .env 中相同的 Broker；如果服务器换了 Broker，
// ESP32 这里也必须一起改。
static const char *WIFI_SSID = "北离";
static const char *WIFI_PASS = "@Ww520527";
static const char *MQTT_HOST = "broker.emqx.io";
static const uint16_t MQTT_PORT = 1883;

// DEVICE_ID 是“当前这台小手机”的设备名，用来判断消息是不是自己发的。
// 例如第一台可以叫 phone_a，第二台可以叫 phone_b。
// 服务器下行消息里的 from == DEVICE_ID 时，main.cpp 会显示右侧气泡。
// static const char *DEVICE_ID = "phone_a";
static const char *DEVICE_ID = "phone_b";

// ROOM_ID 是聊天室 ID。HTTP 历史同步和 MQTT topic 都围绕这个房间号工作。
static const char *ROOM_ID = "room001";

// ESP32 只发布到 up topic。服务器收到后负责保存数据库、分配 seq，
// 然后再把标准消息发布到 down topic。
static const char *MQTT_UP_TOPIC = "chat/room001/up";

// ESP32 只订阅 down topic。这里收到的是服务器处理后的标准聊天 JSON，
// 包含 seq/from/text/ts 等字段，可以安全用于 UI 显示和去重。
static const char *MQTT_DOWN_TOPIC = "chat/room001/down";

// 历史消息 HTTP 服务器地址，必须写电脑在局域网里的 IP，不能写 localhost。
// 对 ESP32 来说 localhost 指 ESP32 自己，不是你的 Windows 电脑。
// 如果换了电脑、路由器重新分配了 IP，后续只需要改这里。
static const char *HTTP_SERVER_BASE = "http://192.168.1.10:3000";

// 保留旧名字，避免其它文件还引用 TOPIC_TX/TOPIC_RX 时失效。
// 新协议下 TX=up，RX=down。
static const char *TOPIC_TX = MQTT_UP_TOPIC;
static const char *TOPIC_RX = MQTT_DOWN_TOPIC;

struct ChatMessage {
    // seq 是服务器保存数据库后分配的递增消息编号，ESP32 不自己生成。
    uint32_t seq = 0;

    // room 是聊天室 ID，用来过滤不属于当前房间的消息。
    String room;

    // client_msg_id 是客户端发送时生成的“单条消息唯一 ID”，不是设备固定 ID。
    // 服务器可以用它避免同一条消息被重复保存。
    String clientMsgId;

    // from 是消息发送方，例如 phone_a / phone_b / pc_test。
    // main.cpp 根据 from 是否等于 DEVICE_ID 决定左右气泡。
    String from;

    // text 是真正显示到聊天气泡里的正文。
    String text;

    // ts 是服务器时间戳，单位是秒。
    uint32_t ts = 0;
};

using ChatMessageCallback = void (*)(const ChatMessage& msg);

class WifiMqttManager {
public:
    using MessageCallback = void (*)(const char *topic, const String &payload);
    using StateCallback   = void (*)(const bool connected);

    WifiMqttManager();

    void begin(const char *ssid,
               const char *pass,
               const char *host,
               uint16_t port,
               const char *topicTx,
               const char *topicRx);

    void update();

    void startWifi();
    void restartWifi();

    bool connectMqtt();
    void disconnectMqtt();

    // 兼容旧接口：直接发布原始 payload。正式聊天消息请使用 publishChat()，
    // 因为服务器 up topic 只应该收到符合协议的 JSON。
    bool publish(const char *payload);

    bool publishChat(const String& text);
    bool syncHistory(uint32_t sinceSeq, uint16_t limit = 100);

    bool wifiConnected() const;
    bool mqttConnected();
    IPAddress localIP() const;

    uint32_t lastLocalSeq() const { return _lastLocalSeq; }
    const String& deviceId() const { return _deviceId; }

    void onMessage(MessageCallback cb);
    void setChatMessageCallback(ChatMessageCallback cb);
    void onWifiStateChange(StateCallback cb);
    void onMqttStateChange(StateCallback cb);

private:
    static WifiMqttManager *s_instance;
    static void mqttCallbackRouter(char *topic, byte *payload, unsigned int length);

    void handleMqttMessage(char *topic, byte *payload, unsigned int length);
    bool parseAndHandleChatJson(const String& payload, bool fromHistory);
    bool handleChatMessage(const ChatMessage& msg, bool fromHistory);

    String nextClientMsgId();
    void loadPersistentState();
    void saveLastLocalSeq();
    void initChipSuffix();

    void emitWifiState(bool connected);
    void emitMqttState(bool connected);

private:
    WiFiClient   _wifiClient;
    PubSubClient _mqtt;

    const char *_ssid    = nullptr;
    const char *_pass    = nullptr;
    const char *_host    = nullptr;
    uint16_t    _port    = 1883;
    const char *_topicTx = nullptr;
    const char *_topicRx = nullptr;

    String _deviceId;
    String _chipSuffix;

    uint32_t _msgCounter = 0;
    uint32_t _lastLocalSeq = 0;

    bool _lastWifiConnected = false;
    bool _lastMqttConnected = false;
    bool _stateLoaded = false;

    uint32_t _wifiConnectStartMs = 0;
    bool _wifiFailureLogged = false;

    MessageCallback _msgCb  = nullptr;
    ChatMessageCallback _chatCb = nullptr;
    StateCallback   _wifiCb = nullptr;
    StateCallback   _mqttCb = nullptr;
};
