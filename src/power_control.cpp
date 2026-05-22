#include "power_control.h"

// power_control.cpp
// 电源控制实现：短按开机由硬件触发，本文件负责上电后接管 CTL，并检测 F+/GPIO27 长按关机。

// 最近一次原始采样的按键状态；true 表示 GPIO27 读到 LOW，即按键正在按下。
static bool s_lastRawPressed = false;

// 消抖后的稳定按键状态；长按时间只基于这个稳定状态判断。
static bool s_stablePressed = false;

// 关机触发标志；用于避免同一次长按重复进入 powerOffNow()。
static bool s_powerOffTriggered = false;

// 最近一次原始按键状态发生变化的时间，用于计算防抖间隔。
static uint32_t s_lastRawChangeMs = 0;

// 稳定按下开始时间；松开时清零，重新按下时重新计时。
static uint32_t s_pressedStartMs = 0;

// 初始化电源控制引脚和内部状态；必须在 setup() 中尽早调用。
// 输入/输出：无参数、无返回值；通过 GPIO14 输出 CTL 保持电平，通过 GPIO27 读取 F+。
void powerControlBegin()
{
    // 短按开机是硬件完成的；ESP32 启动后，软件要尽快接管 CTL。
    // CTL = GPIO14 / P14，先配置成输出，再给明确保持电平，避免悬空或 0.66V 半导通。
    // CTL 必须先设为输出，再输出确定电平，避免上电悬空造成误导通。
    pinMode(POWER_CTL_PIN, OUTPUT);
    // 短按开机由硬件完成；软件启动后用保持电平维持后级电源导通。
    digitalWrite(POWER_CTL_PIN, POWER_HOLD_LEVEL);

    // F+ = GPIO27 / P27，按键按下为 LOW，未按下为 HIGH。
    // 原理图上 F+ 有 10k 上拉到 3.3V；这里使用 INPUT_PULLUP 作为弱上拉兜底。
    // 注意确认 F+ 不会被外部拉到超过 3.3V，ESP32 GPIO 不能承受过压。
    // F+ 作为按键输入；使用内部上拉作为兜底，按下时读到 LOW。
    pinMode(POWER_KEY_PIN, INPUT_PULLUP);

    // 记录初始化时刻和当前按键状态，避免第一次 update() 使用未初始化状态。
    const uint32_t now = millis();
    // 原始状态直接来自 GPIO27；LOW 表示 F/F+ 外部按键被按下。
    s_lastRawPressed = (digitalRead(POWER_KEY_PIN) == LOW);
    // 初始化时先认为当前原始状态就是稳定状态。
    s_stablePressed = s_lastRawPressed;
    // 上电初始化时尚未触发关机。
    s_powerOffTriggered = false;
    // 以当前时间作为第一次防抖计时起点。
    s_lastRawChangeMs = now;
    // 如果开机时按键仍保持按下，则从当前时刻开始累计长按时间。
    s_pressedStartMs = s_stablePressed ? now : 0;
}

// 周期性更新按键状态；必须在 loop() 中反复调用。
// 输入/输出：无参数、无返回值；长按满足条件时会调用 powerOffNow() 并停止业务逻辑。
void powerControlUpdate()
{
    // 当前时间用于防抖和长按时长判断。
    const uint32_t now = millis();
    // 原始采样：F+ 被按下时 GPIO27 为 LOW。
    const bool rawPressed = (digitalRead(POWER_KEY_PIN) == LOW);

    // 原始状态发生变化时，只记录新状态和变化时间，等待防抖时间确认。
    if (rawPressed != s_lastRawPressed) {
        s_lastRawPressed = rawPressed;
        s_lastRawChangeMs = now;
    }

    // 防抖：原始状态保持超过 POWER_DEBOUNCE_MS 后，才更新稳定状态。
    if ((now - s_lastRawChangeMs) >= POWER_DEBOUNCE_MS &&
        rawPressed != s_stablePressed) {
        s_stablePressed = rawPressed;
        // 稳定按下时记录起点；稳定松开时清零，下一次按下重新计时。
        s_pressedStartMs = s_stablePressed ? now : 0;
        // 松开后允许下一次长按重新触发关机。
        s_powerOffTriggered = false;
    }

    // 长按关机：稳定按下超过阈值后，只触发一次 powerOffNow()。
    if (s_stablePressed &&
        !s_powerOffTriggered &&
        (now - s_pressedStartMs) >= POWER_LONG_PRESS_MS) {
        // 先置位触发标志，防止极端情况下重复进入关机流程。
        s_powerOffTriggered = true;
        powerOffNow();
    }
}

// 立即释放 CTL 执行关机；可由长按检测或其它模块主动调用。
// 输入/输出：无参数、无返回值；如果电源未立刻掉电，会停在安全循环中。
void powerOffNow()
{
    // 先输出日志并 flush，尽量保证掉电前串口信息发出。
    Serial.println("[POWER] long press, power off");
    Serial.flush();

    // 长按关机：软件检测 GPIO27/F+ 后释放 CTL，让后级电源自保持断开。
    // 将 CTL 输出为关机电平，释放后级电源自保持。
    digitalWrite(POWER_CTL_PIN, POWER_OFF_LEVEL);

    // 如果电源没有立刻掉电，停在安全状态，避免继续运行 UI/MQTT/输入法等业务逻辑。
    // 电源没有立刻断开时，停在安全循环，避免继续执行其它业务代码。
    while (true) {
        delay(100);
    }
}
