#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>

/******
 * inline：放在头文件里不容易引发多重定义问题
 * constexpr：表示编译期常量
 * const:不会误改字符串常量
 * 
 * ***** */

// inline constexpr const char *WIFI_SSID = "E-FALCON";

static const char *WIFI_SSID = "北离";          //没有static这个修饰会报错，这个是放在头文件.h中，如果还有其它.cpp文件包含了
static const char *WIFI_PASS = "@Ww520527";     //WIFI_MQTT.h文件的话，就每一个.cpp都会有一个WIFI_SSID的定义，这样就会被再重复定义而报错
static const char *MQTT_HOST = "192.168.28.63";  //北离

// static const char *WIFI_SSID = "E-FALCON";   //创建一个指针变量，指向字符串字面量"E-FALCON"，它是只读，不应该被修改
// static const char *WIFI_PASS = "zhongsun666.";
// static const char *MQTT_HOST = "192.168.0.6";   //E-FALCON

static const uint16_t MQTT_PORT = 1883;

static const char *TOPIC_TX = "oubeili/esp32demo/tx";
static const char *TOPIC_RX = "oubeili/esp32demo/rx";

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

    bool publish(const char *payload);

    bool wifiConnected() const;
    bool mqttConnected();
    IPAddress localIP() const;

    void onMessage(MessageCallback cb);
    void onWifiStateChange(StateCallback cb);
    void onMqttStateChange(StateCallback cb);

private:
    static WifiMqttManager *s_instance;
    static void mqttCallbackRouter(char *topic, byte *payload, unsigned int length);

    void handleMqttMessage(char *topic, byte *payload, unsigned int length);
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

    bool _lastWifiConnected = false;
    bool _lastMqttConnected = false;

    MessageCallback _msgCb  = nullptr;
    StateCallback   _wifiCb = nullptr;
    StateCallback   _mqttCb = nullptr;
};