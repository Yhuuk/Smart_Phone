#include <Arduino.h>
#include <lvgl.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "keypad.h"
#include "WIFI_MQTT.h"
#include "nine_key_ime.h"
#include <Preferences.h>

void lvgl_port_init();

// =========================
// 1. 全局变量
// =========================
char k;
MatrixKeypad4x4_CR key_cr;     // 全局键盘对象
WifiMqttManager net;
NineKeyIME ime;

lv_obj_t *text = nullptr;          // 输入框（显示已确认文本）
lv_obj_t *labelStatus = nullptr;   // 状态
lv_obj_t *labelRx = nullptr;       // 接收显示
lv_obj_t *wifiBox = nullptr;       // Wi-Fi 状态框
lv_obj_t *wifiNum = nullptr;       // Wi-Fi 状态文字
lv_obj_t *labelMode = nullptr;     // 输入模式，汉字，英文，数字
lv_obj_t *labelCode = nullptr;     // 当前数字串
lv_obj_t *labelCand = nullptr;     // 汉字候选显示
lv_obj_t *labelPy = nullptr;       // 拼音候选

LV_FONT_DECLARE(ui_font_HanSans_cn_18);
LV_FONT_DECLARE(ui_font_HanSans_cn_16_495);

void set_status(const char *msg);
void set_wifi_box(bool connected);
void refresh_ime_ui();
void mqtt_publish_text();
void Key_input(void);
void ui_create(void);



/************************************ 
NVS= Non-Volatile Strorage:非易失性存储
非易失性存储:数据是存在flash中的，不是存在RAM里的
ime是命名空间，hanzi是键值(也就是索引)
************************************/

Preferences prefs;

void load_ime_text(){
    prefs.begin("ime",false);
    String saved = prefs.getString("hanzi","");
    prefs.end();

    ime.setText(saved);
}

void saved_ime_text(){
    prefs.begin("ime",false);
    prefs.putString("hanzi",ime.text());   //这里的text是自己给的命名，是索引的意思(键值对中的索引)
    prefs.end();
}

void clear_ime_text(){
    prefs.begin("ime",false);
    prefs.putString("hanzi","");   //这里的text是自己给的命名，是索引的意思(键值对中的索引)
    prefs.end();
}

// =========================
// 2. 网络回调
// =========================
void on_wifi_state(bool connected)
{
    set_wifi_box(connected);

    if (connected) {
        set_status("WiFi connected");
        Serial.print("IP = ");
        Serial.println(net.localIP());

        Serial.print("Gateway = ");
        Serial.println(WiFi.gatewayIP());

        Serial.print("Mask = ");
        Serial.println(WiFi.subnetMask());

        Serial.print("MAC = ");
        Serial.println(WiFi.macAddress());
        Serial.print("RSSI = ");
        Serial.println(WiFi.RSSI());

        // C 键改给输入法模式切换，这里在 Wi-Fi 连上后自动尝试一次 MQTT
        if (!net.connectMqtt()) {
            set_status("MQTT connect fail");
            Serial.println("MQTT connect fail");
        }
    } else {
        set_status("WiFi lost");
    }
}

void on_mqtt_state(bool connected)
{
    if (connected) {
        set_status("MQTT connected");
        Serial.println("MQTT connected");
    } else {
        set_status("MQTT offline");
    }
}

void on_mqtt_message(const char *topic, const String &payload)
{
    Serial.print("MQTT RX topic = ");
    Serial.print(topic);
    Serial.print(" , payload = ");
    Serial.println(payload);

    if (labelRx) {
        String s = "接收: ";
        s += payload;
        lv_label_set_text(labelRx, s.c_str());
    }
}

// =========================
// 3. UI 辅助函数
// =========================
void set_status(const char *msg)
{
    if (labelStatus) {
        lv_label_set_text(labelStatus, msg);
    }
    Serial.println(msg);
}

void set_wifi_box(bool connected)
{
    if (!wifiBox || !wifiNum) return;

    if (connected) {
        lv_label_set_text(wifiNum, "成功");
        lv_obj_set_style_bg_color(wifiBox, lv_palette_main(LV_PALETTE_GREEN), 0);
    } else {
        lv_label_set_text(wifiNum, "失败");
        lv_obj_set_style_bg_color(wifiBox, lv_palette_main(LV_PALETTE_RED), 0);
    }
}

void refresh_ime_ui()
{
    if (text) {
        lv_textarea_set_text(text, ime.text().c_str());
        lv_textarea_set_cursor_pos(text, ime.textCursor());
    }

    if (labelMode) {
        String s = "模式: ";
        s += ime.modeName();
        lv_label_set_text(labelMode, s.c_str());
    }

    if (labelCode) {
        String s = "编码: ";
        s += ime.digits();
        lv_label_set_text(labelCode, s.c_str());
    }

    // if (labelCand) {
    //     String s = "候选: ";
    //     s += ime.candidatesPreview(6);
    //     lv_label_set_text(labelCand, s.c_str());
    // }

    /***************2.0新增********************** */
    if (labelPy) {
    String s = "拼音: ";
    s += ime.pyCandidatesPreview(4);
    lv_label_set_text(labelPy, s.c_str());
    }

    if (labelCand) {
        String s = "汉字: ";
        s += ime.hzCandidatesPreview(6);
        lv_label_set_text(labelCand, s.c_str());
    }
    /***************2.0新增********************** */
}

void mqtt_publish_text()
{
    String payload = ime.text();

    if (payload.length() == 0) {
        set_status("Input empty");
        return;
    }

    bool ok = net.publish(payload.c_str());  //这个发布话题也是将String转化为.c_str()
    if (ok) {
        set_status("Publish OK");
        Serial.print("MQTT TX: ");
        Serial.println(payload);

        // clear_ime_text();
       
        ime.clearText();
        ime.clearComposition();
        saved_ime_text();

        refresh_ime_ui();
    } else {
        set_status("Publish FAIL");
    }
}

// =========================
// 4. 创建界面
// =========================
void ui_create(void)
{
    // 状态标签
    labelStatus = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(labelStatus, &ui_font_HanSans_cn_18, 0);
    lv_label_set_text(labelStatus, "系统启动");
    lv_obj_align(labelStatus, LV_ALIGN_TOP_LEFT, 10, 8);

    // 模式标签
    labelMode = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(labelMode, &ui_font_HanSans_cn_18, 0);
    lv_label_set_text(labelMode, "模式: 中文");
    lv_obj_align(labelMode, LV_ALIGN_TOP_LEFT, 10, 36);

    // 编码标签
    labelCode = lv_label_create(lv_scr_act());
    lv_obj_set_style_text_font(labelCode, &ui_font_HanSans_cn_18, 0);
    lv_label_set_text(labelCode, "编码: ");
    lv_obj_align(labelCode, LV_ALIGN_TOP_LEFT, 10, 64);

    // 右上角 Wi-Fi 状态框
    wifiBox = lv_obj_create(lv_scr_act());
    lv_obj_set_size(wifiBox, 60, 40);
    lv_obj_align(wifiBox, LV_ALIGN_TOP_RIGHT, -10, 10);
    lv_obj_set_style_radius(wifiBox, 5, 0);
    lv_obj_set_style_border_width(wifiBox, 1, 0);
    lv_obj_set_style_pad_all(wifiBox, 0, 0);

    wifiNum = lv_label_create(wifiBox);
    lv_obj_set_style_text_font(wifiNum, &ui_font_HanSans_cn_18, 0);
    lv_label_set_text(wifiNum, "失败");
    lv_obj_center(wifiNum);
    set_wifi_box(false);

    // 输入框：显示已确认文本
    text = lv_textarea_create(lv_scr_act());
    lv_obj_set_size(text, LV_PCT(90), 78);
    lv_obj_align(text, LV_ALIGN_TOP_MID, 0, 92);
    lv_obj_add_state(text, LV_STATE_FOCUSED);    //添加焦点，有光标
    lv_obj_set_style_text_font(text, &ui_font_HanSans_cn_18, 0);
    lv_textarea_set_text(text, "");              //清空文本
    lv_textarea_set_placeholder_text(text, "输入区");   //占位提示文本(在真正输入文本时，会有有一个灰色文字提示)
    lv_textarea_set_cursor_click_pos(text, false);      //控制光标位置
    lv_obj_set_scrollbar_mode(text, LV_SCROLLBAR_MODE_OFF);  //不显示滚轮条

    // // 候选标签，旧版1.0
    // labelCand = lv_label_create(lv_scr_act());
    // lv_obj_set_width(labelCand, LV_PCT(90));
    // lv_obj_set_style_text_font(labelCand, &ui_font_HanSans_cn_18, 0);
    // lv_obj_set_style_text_color(labelCand, lv_color_hex(0x00AA00), 0);
    // lv_label_set_long_mode(labelCand, LV_LABEL_LONG_WRAP);     //一行字体满了自动换行
    // lv_label_set_text(labelCand, "候选: 无");
    // lv_obj_align_to(labelCand, text, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);

    /***********************新版2.0**************************** */
    // 拼音候选标签
    labelPy = lv_label_create(lv_scr_act());
    lv_obj_set_width(labelPy, LV_PCT(90));
    lv_obj_set_style_text_font(labelPy, &ui_font_HanSans_cn_18, 0);
    lv_obj_set_style_text_color(labelPy, lv_color_hex(0x0066CC), 0);
    lv_label_set_long_mode(labelPy, LV_LABEL_LONG_WRAP);
    lv_label_set_text(labelPy, "拼音: 无");
    lv_obj_align_to(labelPy, text, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 8);

    // 汉字候选标签
    labelCand = lv_label_create(lv_scr_act());
    lv_obj_set_width(labelCand, LV_PCT(90));
    lv_obj_set_style_text_font(labelCand, &ui_font_HanSans_cn_18, 0);
    lv_obj_set_style_text_color(labelCand, lv_color_hex(0x00AA00), 0);
    lv_label_set_long_mode(labelCand, LV_LABEL_LONG_WRAP);
    lv_label_set_text(labelCand, "汉字: 无");
    lv_obj_align_to(labelCand, labelPy, LV_ALIGN_OUT_BOTTOM_LEFT, 0, 6);
    /***********************新版2.0**************************** */


    // 接收标签
    labelRx = lv_label_create(lv_scr_act());
    lv_obj_set_width(labelRx, LV_PCT(90));
    lv_obj_set_style_text_font(labelRx, &ui_font_HanSans_cn_18, 0);
    lv_label_set_long_mode(labelRx, LV_LABEL_LONG_WRAP);
    lv_label_set_text(labelRx, "接收: ");
    lv_obj_set_style_text_color(labelRx, lv_color_hex(0xff0000), 0);
    lv_obj_set_style_bg_color(labelRx, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(labelRx, LV_OPA_COVER, 0);
    lv_obj_align(labelRx, LV_ALIGN_BOTTOM_LEFT, 10, 2);   //之前是-6

}

// =========================
// 5. 按键处理
// 说明：
// 2~9 -> 九键拼音输入
// A   -> 上一个候选
// B   -> 下一个候选
// C   -> 中文/英文/数字切换
// D   -> 确认候选
// 0   -> 当前无编码时，返回预留
// *   -> 删除
// #   -> 发送
// =========================
void Key_input(void)
{
    if (key_cr.getKey(k))
    {
        Serial.print("Key = ");
        Serial.println(k);

        String oldText = ime.text();

        ime.onKey(k);

        if(ime.text() != oldText){
            saved_ime_text();
        }

        refresh_ime_ui();

        if (ime.takeSendFlag()) {
            mqtt_publish_text();
        }

        if (ime.takeBackFlag()) {
            set_status("Back reserved");
        }
    }
}

// =========================
// 6. setup
// =========================
void setup(void)
{
    Serial.begin(115200);

    lvgl_port_init();

    key_cr.begin(25);
    key_cr.enableRepeat('*', 500, 80);    //长按按键

    pinMode(21, OUTPUT);
    digitalWrite(21, HIGH);

    ime.begin();
    ui_create();
    refresh_ime_ui();
    //加载Flash中的内容，并且显示在ttf显示器上
    load_ime_text();
    lv_textarea_set_text(text, ime.text().c_str());
    lv_textarea_set_cursor_pos(text, ime.textCursor());


    net.begin(WIFI_SSID, WIFI_PASS, MQTT_HOST, MQTT_PORT, TOPIC_TX, TOPIC_RX);
    net.onMessage(on_mqtt_message);
    net.onWifiStateChange(on_wifi_state);
    net.onMqttStateChange(on_mqtt_state);
    net.startWifi();
    set_status("WiFi connecting...");
}

// =========================
// 7. loop
// =========================
void loop()
{
    static uint32_t last = millis();
    uint32_t now = millis();
    lv_tick_inc(now - last);
    last = now;

    // 1. 先扫键盘
    Key_input();

    // 2. 再更新网络状态（轻量）
    net.update();

    // 3. 最后跑 LVGL
    lv_timer_handler();

    delay(5);
}
