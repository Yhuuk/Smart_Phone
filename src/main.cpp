#include <Arduino.h>
#include <lvgl.h>

#include "chat_ui.h"
#include "keypad.h"
#include "nine_key_ime.h"
#include "WIFI_MQTT.h"

// lvgl_port.cpp 里面已经有真正的 TFT_eSPI + LVGL 初始化。
// 这里不要再重复创建 TFT_eSPI，也不要再重复 lv_init()。
extern void lvgl_port_init();

static MatrixKeypad4x4_CR keypad;
static NineKeyIME ime;
static WifiMqttManager net;

static uint32_t lastMqttTryMs = 0;
static uint32_t lastStatusUiMs = 0;

static void refreshImeUi()
{
    String mode = ime.modeName();
    ui_chat_setMode(mode.c_str());

    ui_chat_setInputText(ime.text().c_str());

    String py;
    String hz;

    if (mode == "中文") {
        py = ime.pyCandidatesPreview(4);
        hz = ime.hzCandidatesPreview(6);

        // 有些编码阶段还没有候选时，至少把数字串显示出来，方便判断按键是否进来了。
        if (py.length() == 0 && ime.digits().length() > 0) {
            py = ime.digits();
        }
    } else {
        // 英文 / 数字模式下，不在上面的拼音候选区显示 en/num。
        // 顶部状态栏已经显示 cn / en / num，这里保持两层输入法候选区为空，避免界面重影和信息重复。
        py = ime.hzCandidatesPreview(8);;
        hz = "";
    }

    ui_chat_setIme(py.c_str(), hz.c_str());
}

static void onWifiState(bool connected)
{
    ui_chat_setWifi(connected);
    Serial.print("[WIFI] ");
    Serial.println(connected ? "connected" : "disconnected");
}

static void onMqttState(bool connected)
{
    ui_chat_setMqtt(connected);
    Serial.print("[MQTT] ");
    Serial.println(connected ? "connected" : "disconnected");
}

static void onMqttMessage(const char *topic, const String &payload)
{
    Serial.print("[MQTT RX] ");
    Serial.print(topic);
    Serial.print(" => ");
    Serial.println(payload);

    ui_chat_addMessage(payload.c_str(), false);
}

static void tryConnectMqtt()
{
    if (!net.wifiConnected()) return;
    if (net.mqttConnected()) return;

    uint32_t now = millis();
    if (now - lastMqttTryMs < 3000) return;
    lastMqttTryMs = now;

    Serial.println("[MQTT] trying connect...");
    bool ok = net.connectMqtt();
    Serial.println(ok ? "[MQTT] connect ok" : "[MQTT] connect failed");
}

static void sendCurrentText()
{
    String msg = ime.text();
    msg.trim();

    if (msg.length() == 0) {
        refreshImeUi();
        return;
    }

    if (!net.wifiConnected()) {
        ui_chat_addMessage("WiFi未连接", false);
        refreshImeUi();
        return;
    }

    if (!net.mqttConnected()) {
        ui_chat_addMessage("MQTT未连接", false);
        tryConnectMqtt();
        refreshImeUi();
        return;
    }

    bool ok = net.publish(msg.c_str());
    if (ok) {
        ui_chat_addMessage(msg.c_str(), true);
        ime.clearText();
    } else {
        ui_chat_addMessage("发送失败", false);
    }

    refreshImeUi();
}

static void processImeKey(char k)
{
    ime.onKey(k);
    refreshImeUi();

    if (ime.takeSendFlag()) {
        sendCurrentText();
    }

    if (ime.takeBackFlag()) {
        // 杩欓噷鍏堜笉鍋氶〉闈㈣繑鍥烇紝閬垮厤璇垹鍔熻兘銆?
        // 浠ュ悗浣犺鍋氣€滆繑鍥炰笂涓€椤碘€濓紝灏卞湪杩欓噷鎺ラ〉闈㈠垏鎹㈤€昏緫銆?
        Serial.println("[IME] back flag");
    }
}

static void handleKeypad()
{
    static bool pendingAbKey = false;
    static char pendingAbChar = 0;
    static bool pendingAbScrolled = false;

    KeyEvent event;
    while (keypad.poll(event)) {
        char k = event.key;

        if (event.type == KeyEventType::Released) {
            if ((k == 'A' || k == 'B') && pendingAbKey && pendingAbChar == k) {
                if (!pendingAbScrolled) {
                    processImeKey(k);
                }

                pendingAbKey = false;
                pendingAbChar = 0;
                pendingAbScrolled = false;
            }
            continue;
        }

        Serial.print("[KEY] ");
        Serial.println(k);

        if (event.type == KeyEventType::Pressed && (k == 'A' || k == 'B')) {
            pendingAbKey = true;
            pendingAbChar = k;
            pendingAbScrolled = false;
            continue;
        }

        if (event.type == KeyEventType::Repeat && (k == 'A' || k == 'B')) {
            pendingAbScrolled = true;
            ui_chat_scrollMessages(k == 'A' ? -1 : 1);
            continue;
        }

        ime.onKey(k);
        refreshImeUi();

        if (ime.takeSendFlag()) {
            sendCurrentText();
        }

        if (ime.takeBackFlag()) {
            // 这里先不做页面返回，避免误删功能。
            // 以后你要做“返回上一页”，就在这里接页面切换逻辑。
            Serial.println("[IME] back flag");
        }
    }
}

static void updateStatusUiPeriodically()
{
    uint32_t now = millis();
    if (now - lastStatusUiMs < 500) return;
    lastStatusUiMs = now;

    ui_chat_setWifi(net.wifiConnected());
    ui_chat_setMqtt(net.mqttConnected());
    ui_chat_setBattery(82);   // 目前还是固定电量；后面接 ADC 后再替换这里。
}

void setup()
{
    Serial.begin(115200);
    delay(200);

    lvgl_port_init();

    ui_chat_create();

    keypad.begin(25);
    keypad.enableRepeat('*', 500, 80);
    keypad.enableRepeat('A', 500, 220);
    keypad.enableRepeat('B', 500, 220);

    ime.begin();
    refreshImeUi();

    net.begin(WIFI_SSID,
              WIFI_PASS,
              MQTT_HOST,
              MQTT_PORT,
              TOPIC_TX,
              TOPIC_RX);

    net.onWifiStateChange(onWifiState);
    net.onMqttStateChange(onMqttState);
    net.onMessage(onMqttMessage);

    net.startWifi();

    ui_chat_setWifi(false);
    ui_chat_setMqtt(false);
    ui_chat_setBattery(82);

    Serial.println("[BOOT] ESP32 phone started");
}

void loop()
{
    lv_tick_inc(5);
    lv_timer_handler();

    handleKeypad();

    net.update();
    tryConnectMqtt();
    updateStatusUiPeriodically();

    delay(5);
}
