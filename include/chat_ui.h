#pragma once

#include <Arduino.h>
#include <lvgl.h>

void ui_chat_create();

void ui_chat_setMode(const char *mode);
void ui_chat_setWifi(bool connected);
void ui_chat_setMqtt(bool connected);
void ui_chat_setBattery(uint8_t percent);

void ui_chat_setIme(const char *pinyin, const char *hanzi);
void ui_chat_setInputText(const char *text);
const char *ui_chat_getInputText();

void ui_chat_addMessage(const char *text, bool mine);
void ui_chat_clearMessages();
void ui_chat_scrollMessages(int8_t direction);
