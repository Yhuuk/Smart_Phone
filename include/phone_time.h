#pragma once

#include <Arduino.h>
#include <stddef.h>

// phone_time 模块只负责“获取和格式化真实时间”，不直接操作 LVGL。
// 这样 UI 层只需要调用 ui_chat_setClock() 显示字符串，后续如果要换成
// DS3231 RTC、服务器下发时间，或者其它校时方式，只需要替换本模块内部实现。

// 初始化时间模块。
// 这里不会阻塞等待 NTP，也不会要求 WiFi 已经连接；真正的 NTP 启动放在
// phone_time_update() 中按 WiFi 状态触发。
void phone_time_begin();

// 根据当前 WiFi 状态推进时间同步。
// - wifiConnected == true 时：启动或重试 NTP 同步。
// - wifiConnected == false 时：不做网络操作；如果之前已经同步过，ESP32
//   的系统时间仍会继续走，界面可以继续显示当前本地时间。
void phone_time_update(bool wifiConnected);

// 把当前本地时间格式化成两行顶部栏文字。
//
// dateBuf 输出示例：2026-06-07
// timeBuf 输出示例：Sun 14:23
//
// 返回 true 表示系统时间已经可信；返回 false 表示还没从 NTP 或其它来源
// 拿到真实时间，调用方应该显示占位符。
bool phone_time_format(char *dateBuf,
                       size_t dateLen,
                       char *timeBuf,
                       size_t timeLen);

// 当前系统时间是否已经同步到可信范围。
bool phone_time_ready();
