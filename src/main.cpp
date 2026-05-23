#include <Arduino.h>
#include <lvgl.h>

#include "chat_ui.h"
#include "keypad.h"
#include "nine_key_ime.h"
#include "WIFI_MQTT.h"
#include "power_control.h"

// lvgl_port.cpp 里面已经有真正的 TFT_eSPI + LVGL 初始化。
// 这里不要再重复创建 TFT_eSPI，也不要再重复 lv_init()。
extern void lvgl_port_init();

static MatrixKeypad4x4_CR keypad;
static NineKeyIME ime;
static WifiMqttManager net;

static uint32_t lastMqttTryMs = 0;
static uint32_t lastStatusUiMs = 0;
static bool historySyncAttempted = false;

static void refreshImeUi()
{
    InputMode mode = ime.mode();
    String modeName = ime.modeName();
    ui_chat_setMode(modeName.c_str());

    ui_chat_setInputText(ime.text().c_str());
    ui_chat_setIme("", "");

    if (mode == INPUT_CN) {
        if (ime.pyCandidateCount() > 0) {
            ui_chat_setImeCandidates(IME_ROW_TOP,
                                      ime.pyCandidates(),
                                      ime.pyCandidateCount(),
                                      ime.pySelectedIndex(),
                                      4,
                                      IME_COLOR_PINYIN);
        } else if (ime.digits().length() > 0) {
            // 有些编码阶段还没有候选时，至少把数字串显示出来，方便判断按键是否进来了。
            ui_chat_setImeRowText(IME_ROW_TOP, ime.digits().c_str());
        }

        if (ime.hzCandidateCount() > 0) {
            ui_chat_setImeCandidates(IME_ROW_BOTTOM,
                                      ime.hzCandidates(),
                                      ime.hzCandidateCount(),
                                      ime.hzSelectedIndex(),
                                      6,
                                      IME_COLOR_HANZI);
        }
    } else if (mode == INPUT_EN) {
        if (ime.hzCandidateCount() > 0) {
            ui_chat_setImeCandidates(IME_ROW_TOP,
                                      ime.hzCandidates(),
                                      ime.hzCandidateCount(),
                                      ime.hzSelectedIndex(),
                                      8,
                                      IME_COLOR_EN_NUM);
        }
    }
}

static void onWifiState(bool connected)
{
    ui_chat_setWifi(connected);
}

static void onMqttState(bool connected)
{
    ui_chat_setMqtt(connected);
}

static void onChatMessage(const ChatMessage &msg)
{
    // main.cpp 负责 UI、输入法、按键和通信层的衔接。
    // JSON 拼接、解析、去重都放在 WIFI_MQTT.cpp，
    // 避免网络协议细节进入小手机 UI 或九键输入法代码。
    bool mine = (msg.from == DEVICE_ID);
    ui_chat_addMessage(msg.text.c_str(), mine);
}

static void tryConnectMqtt()
{
    if (!net.wifiConnected()) return;
    if (net.mqttConnected()) return;

    uint32_t now = millis();
    if (now - lastMqttTryMs < 3000) return;
    lastMqttTryMs = now;

    net.connectMqtt();
}

static void syncHistoryOnceAfterWifi()
{
    // 开机后 WiFi 一旦连上，先同步历史，再让 loop 继续连接 MQTT。
    // HTTP 同步失败也只记录日志，不阻塞小手机继续使用实时 MQTT。
    if (historySyncAttempted) return;
    if (!net.wifiConnected()) return;

    historySyncAttempted = true;
    net.syncHistory(net.lastLocalSeq(), 100);
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

    bool ok = net.publishChat(msg);
    if (ok) {
        // publish 成功只代表消息发到了 MQTT Broker。
        // 最终聊天气泡等服务器从 down topic 返回带 seq 的标准消息后再显示。
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
        // 这里先不做页面返回，避免误删功能。
        // 以后你要做“返回上一页”，就在这里接页面切换逻辑。
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
    powerControlBegin();

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
    net.setChatMessageCallback(onChatMessage);

    net.startWifi();

    ui_chat_setWifi(false);
    ui_chat_setMqtt(false);
    ui_chat_setBattery(82);

    Serial.println("[BOOT] ESP32 phone started");
}

void loop()
{
    powerControlUpdate();

    lv_tick_inc(5);
    lv_timer_handler();

    handleKeypad();

    net.update();
    syncHistoryOnceAfterWifi();
    tryConnectMqtt();
    updateStatusUiPeriodically();

    delay(5);
}
