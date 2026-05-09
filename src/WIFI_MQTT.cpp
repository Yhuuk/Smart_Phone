#include "WIFI_MQTT.h"

WifiMqttManager *WifiMqttManager::s_instance = nullptr;

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

    _mqtt.setServer(_host, _port);
    _mqtt.setCallback(WifiMqttManager::mqttCallbackRouter);
}

void WifiMqttManager::startWifi()
{
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

    String clientId = "esp32-";
    clientId += String((uint32_t)(ESP.getEfuseMac() & 0xFFFFFFFF), HEX);

    bool ok = _mqtt.connect(clientId.c_str());
    if (ok) {
        _mqtt.subscribe(_topicRx);
        _mqtt.publish(_topicTx, "ESP32 connected");
    }

    bool nowMqtt = _mqtt.connected();
    if (nowMqtt != _lastMqttConnected) {
        _lastMqttConnected = nowMqtt;
        emitMqttState(nowMqtt);
    }

    return ok;
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
    if (!_mqtt.connected()) return false;

    return _mqtt.publish(_topicTx, payload);
}

void WifiMqttManager::update()
{
    bool nowWifi = wifiConnected();
    if (nowWifi != _lastWifiConnected) {
        _lastWifiConnected = nowWifi;
        emitWifiState(nowWifi);

        if (!nowWifi) {
            disconnectMqtt();
        }
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

    if (_msgCb) {
        _msgCb(topic, msg);
    }
}