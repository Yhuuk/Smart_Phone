#pragma once

#include <Arduino.h>

// battery_monitor 模块只负责电池电压采样和电压到百分比的换算。
// 它不直接操作 LVGL，也不关心顶部状态栏怎么画；UI 层只需要读取
// battery_monitor_percent() 的结果，再调用 ui_chat_setBattery()。

// 初始化电池检测相关 GPIO 和 ADC。
//
// 必须在 setup() 中调用一次：
// - DETECT_CONTROL / GPIO13 配置为开漏输出，并默认释放关闭测量电路；
// - BATTERY_DETECT / GPIO33 配置为 ADC 输入；
// - GPIO33 设置为 11dB 衰减，适合读取分压后的单节锂电电压。
void battery_monitor_begin();

// 周期性更新电池读数。
//
// 这个函数可以在 loop() 或已有状态栏刷新函数里反复调用。
// 内部会自己判断是否到了测量时间：
// - 开机后第一次调用会立即测一次；
// - 之后默认每 60 秒测一次；
// - 只有测量期间才会用开漏输出拉低 DETECT_CONTROL；
// - 平时释放为高阻态，由 R26 上拉关断测量电路以省电。
void battery_monitor_update();

// 返回最近一次计算得到的电量百分比，范围固定为 0~100。
//
// 百分比不是简单线性换算，而是根据单节锂电池放电曲线查表并插值。
// 如果还没有完成过测量，返回 0。
uint8_t battery_monitor_percent();

// 返回最近一次估算的电池电压，单位 V。
//
// 这个接口主要用于串口调试或后续低电量保护判断；UI 当前只需要百分比。
// 如果还没有完成过测量，返回 0.0f。
float battery_monitor_voltage();

// 是否已经完成过至少一次有效测量。
bool battery_monitor_hasReading();
