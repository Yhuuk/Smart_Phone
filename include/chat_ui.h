#pragma once

#include <Arduino.h>
#include <lvgl.h>

void ui_chat_create();

void ui_chat_setMode(const char *mode);
void ui_chat_setClock(const char *dateText, const char *timeText);
void ui_chat_setWifi(bool connected);
void ui_chat_setMqtt(bool connected);
void ui_chat_setBattery(uint8_t percent);

enum ImeCandidateRow : uint8_t {
    IME_ROW_TOP = 0,
    IME_ROW_BOTTOM
};

enum ImeCandidateColorRole : uint8_t {
    IME_COLOR_PINYIN = 0,
    IME_COLOR_HANZI,
    IME_COLOR_EN_NUM
};

void ui_chat_setIme(const char *pinyin, const char *hanzi);
void ui_chat_setImeRowText(ImeCandidateRow row, const char *text);
void ui_chat_setImeCandidates(ImeCandidateRow row,
                              const String *items,
                              uint8_t count,
                              uint8_t selected,
                              uint8_t maxShow,
                              ImeCandidateColorRole colorRole);
void ui_chat_setInputText(const char *text, uint16_t cursorPos = 0);
const char *ui_chat_getInputText();

void ui_chat_addMessage(const char *text, bool mine);
void ui_chat_clearMessages();
void ui_chat_scrollMessages(int8_t direction);
