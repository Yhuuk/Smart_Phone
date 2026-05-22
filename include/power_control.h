#pragma once
// power_control.h
// 电源控制模块对外接口：负责 GPIO14/CTL 自保持控制和 GPIO27/F+ 长按关机检测。
// 外部模块通常只需要在 setup() 里调用 powerControlBegin()，在 loop() 里周期性调用 powerControlUpdate()。

#include <Arduino.h>

// 电源控制引脚定义：
// CTL = GPIO14 / P14，连接后级电源自保持控制电路。
// F+  = GPIO27 / P27，外部电源按键检测脚，按键按下为 LOW，未按下为 HIGH。
// CTL 控制引脚号：GPIO14 / P14，输出到后级电源自保持控制电路。
// POWER_CTL_PIN 只在电源控制模块中用于 pinMode() 和 digitalWrite()。
static constexpr uint8_t POWER_CTL_PIN = 14;

// F+ 按键检测引脚号：GPIO27 / P27，按键按下为 LOW，未按下为 HIGH。
// POWER_KEY_PIN 用于读取外部电源按键状态。
static constexpr uint8_t POWER_KEY_PIN = 27;

// 长按关机判定时间，单位毫秒；持续按下超过该时间后执行 powerOffNow()。
static constexpr uint32_t POWER_LONG_PRESS_MS = 1000;

// 按键软件防抖时间，单位毫秒；过滤机械按键抖动导致的误触发。
static constexpr uint32_t POWER_DEBOUNCE_MS = 30;

// CTL 电平集中在这里配置：
// 短按开机由硬件完成，软件启动后负责把 CTL 置为保持电平。
// 长按关机由软件检测 GPIO27/F+ 后，把 CTL 置为释放自保持的电平。
// 如果实测发现开机/关机逻辑反了，只需要交换下面两个常量。
// CTL 保持开机电平：ESP32 启动后输出该电平，用于维持电源自保持。
// 如果实测发现保持/关机逻辑相反，只需要和 POWER_OFF_LEVEL 对调。
static constexpr uint8_t POWER_HOLD_LEVEL = HIGH;

// CTL 释放关机电平：长按关机时输出该电平，让后级电源自保持断开。
static constexpr uint8_t POWER_OFF_LEVEL  = LOW;

// 初始化电源控制引脚和内部状态，必须在 setup() 中尽早调用。
// 作用：第一时间接管 CTL，并配置 F+ 按键输入。
void powerControlBegin();

// 周期性检测 F+ 按键状态，必须在 loop() 中反复调用。
// 作用：完成按键防抖和长按关机判断。
void powerControlUpdate();

// 立即释放 CTL 执行关机；长按触发后会调用，也可供其它模块主动关机时调用。
// 调用后如果电源没有立刻断开，程序会停在安全循环中。
void powerOffNow();
