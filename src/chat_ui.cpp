#include "chat_ui.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/*
   你的中文字体文件：src/ui_font_HanSans_cn_16_495.c
   如果你的字体变量名不是这个，只改下面这一行。
*/
LV_FONT_DECLARE(ui_font_HanSans_cn_10_8);

#define UI_W 240
#define UI_H 320

#define FONT_CN     (&ui_font_HanSans_cn_10_8)
#define FONT_BATT   FONT_CN
#define FONT_SMALL  (&lv_font_montserrat_14)

// =====================
// 颜色：按你给的 240x320 UI 图接近配置
// =====================
static const uint32_t C_SCREEN_BG     = 0x66C2C5;
static const uint32_t C_TOP_BAR       = 0x6540D9;

static const uint32_t C_CHAT_PANEL    = 0xDB62A1;
static const uint32_t C_BUBBLE_LEFT   = 0xB934E5;
static const uint32_t C_BUBBLE_RIGHT  = 0x16E1E8;

static const uint32_t C_ROW_BG        = 0x51DAB6;
static const uint32_t C_BOTTOM_BG     = 0x66C2C5;
static const uint32_t C_SEND_BTN      = 0x1EDFEB;

// 候选区选中颜色集中在这里。
// 如果想修改候选区选中颜色，改这里即可。
static const uint32_t C_IME_PINYIN_SEL_BG = 0x1D4ED8;  // 这里是拼音选中背景色
static const uint32_t C_IME_HANZI_SEL_BG  = 0xF97316;  // 这里是汉字选中背景色
static const uint32_t C_IME_EN_NUM_SEL_BG = 0x7C3AED;  // 这里是英文/数字选中背景色
static const uint32_t C_IME_SEL_TEXT      = 0xFFFFFF;  // 这里是选中候选文字颜色

static const uint32_t C_WHITE         = 0xFFFFFF;
static const uint32_t C_GREEN         = 0x22F05A;
static const uint32_t C_BATTERY_LOW   = 0xFF3030;
static const uint32_t C_BATTERY_TEXT_ON_FILL = 0x001A1A;
static const uint32_t C_GRAY          = 0x808080;
static const uint32_t C_TIME_TEXT     = 0xD8D1EA;
static const uint32_t C_TEXT          = 0x092D32;
static const uint32_t C_AVATAR_PURPLE = 0xA938E3;

// =====================
// 全局 UI 对象
// =====================
static lv_obj_t *g_root = nullptr;
static lv_obj_t *g_chatPanel = nullptr;

static lv_obj_t *g_modeLabel = nullptr;
static lv_obj_t *g_dateLabel = nullptr;
static lv_obj_t *g_timeLabel = nullptr;
static lv_obj_t *g_wifiLabel = nullptr;
static lv_obj_t *g_batteryFill = nullptr;
static lv_obj_t *g_batteryLabel = nullptr;

static lv_obj_t *g_mqttParts[5] = {nullptr, nullptr, nullptr, nullptr, nullptr};

static lv_obj_t *g_pinyinRow = nullptr;
static lv_obj_t *g_hanziRow = nullptr;
static lv_obj_t *g_pinyinLabel = nullptr;
static lv_obj_t *g_hanziLabel = nullptr;
static lv_obj_t *g_inputTa = nullptr;

static constexpr uint8_t IME_MAX_ROW_ITEMS = 8;
static lv_obj_t *g_pinyinCandBox[IME_MAX_ROW_ITEMS] = {nullptr};
static lv_obj_t *g_pinyinCandLabel[IME_MAX_ROW_ITEMS] = {nullptr};
static lv_obj_t *g_hanziCandBox[IME_MAX_ROW_ITEMS] = {nullptr};
static lv_obj_t *g_hanziCandLabel[IME_MAX_ROW_ITEMS] = {nullptr};

static constexpr uint8_t CHAT_MAX = 100;
static String *g_msgText = nullptr;
static bool *g_msgMine = nullptr;
static uint32_t *g_msgTs = nullptr;
static uint8_t g_msgCount = 0;
static bool g_msgBatchMode = false;
static bool g_msgBatchDirty = false;
static int16_t g_msgViewLast = -1;
static bool g_msgAutoFollow = true;

static constexpr int16_t CHAT_PANEL_H = 209;
static constexpr int16_t CHAT_TOP_PAD = 4;
static constexpr int16_t CHAT_GAP = 4;
static constexpr int16_t CHAT_SCROLL_STEP = 42;
static constexpr uint32_t CHAT_TIME_GAP_SEC = 5 * 60;
static constexpr int16_t TIME_LABEL_H = 13;
static constexpr int16_t TIME_AFTER_GAP = 2;
static constexpr int16_t CHAT_BOTTOM_PAD = 4;

static constexpr int16_t MSG_MAX_TEXT_W = 164;
static constexpr int16_t MSG_MIN_TEXT_W = 16;
static constexpr int16_t MSG_PAD_X = 5;
static constexpr int16_t MSG_PAD_Y = 3;
static constexpr int16_t MSG_LINE_SPACE = 1;
static constexpr int16_t MSG_MIN_BUBBLE_W = 26;
static constexpr int16_t MSG_MIN_BUBBLE_H = 20;

static constexpr int16_t IME_ROW_H = 15;

// =====================
// 基础工具
// =====================
static bool ensureMessageStorage()
{
    if (!g_msgText) {
        g_msgText = new String[CHAT_MAX];
    }

    if (!g_msgMine) {
        g_msgMine = new bool[CHAT_MAX]();
    }

    if (!g_msgTs) {
        g_msgTs = new uint32_t[CHAT_MAX]();
    }

    return g_msgText && g_msgMine && g_msgTs;
}

static lv_color_t hex(uint32_t c)
{
    return lv_color_hex(c);
}

static void setBg(lv_obj_t *obj, uint32_t color)
{
    if (!obj) return;
    lv_obj_set_style_bg_color(obj, hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
}

static lv_obj_t *box(lv_obj_t *parent,
                     int16_t x,
                     int16_t y,
                     int16_t w,
                     int16_t h,
                     uint32_t color,
                     int16_t radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);

    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);

    lv_obj_set_style_bg_color(obj, hex(color), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);

    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *borderBox(lv_obj_t *parent,
                           int16_t x,
                           int16_t y,
                           int16_t w,
                           int16_t h,
                           uint32_t borderColor,
                           uint8_t borderWidth,
                           int16_t radius)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_style_all(obj);

    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);

    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_border_color(obj, hex(borderColor), LV_PART_MAIN);
    lv_obj_set_style_border_width(obj, borderWidth, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, radius, LV_PART_MAIN);

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *label(lv_obj_t *parent,
                       const char *txt,
                       int16_t x,
                       int16_t y,
                       int16_t w,
                       int16_t h,
                       const lv_font_t *font,
                       uint32_t color,
                       lv_text_align_t align)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_remove_style_all(obj);

    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);

    lv_label_set_text(obj, txt ? txt : "");
    lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);

    lv_obj_set_style_text_font(obj, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_align(obj, align, LV_PART_MAIN);

    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static lv_obj_t *centerLabel(lv_obj_t *parent,
                             const char *txt,
                             const lv_font_t *font,
                             uint32_t color)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_remove_style_all(obj);

    lv_label_set_text(obj, txt ? txt : "");
    lv_obj_set_style_text_font(obj, font, LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, hex(color), LV_PART_MAIN);
    lv_obj_set_style_text_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);

    lv_obj_center(obj);
    return obj;
}

static lv_obj_t *bubbleLabel(lv_obj_t *parent,
                             const char *txt,
                             int16_t x,
                             int16_t y,
                             int16_t w,
                             int16_t h,
                             uint32_t bgColor,
                             uint32_t textColor)
{
    lv_obj_t *obj = lv_label_create(parent);
    lv_obj_remove_style_all(obj);

    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);

    lv_label_set_text(obj, txt ? txt : "");
    lv_label_set_long_mode(obj, LV_LABEL_LONG_WRAP);

    lv_obj_set_style_bg_color(obj, hex(bgColor), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(obj, 8, LV_PART_MAIN);
    lv_obj_set_style_pad_left(obj, MSG_PAD_X, LV_PART_MAIN);
    lv_obj_set_style_pad_right(obj, MSG_PAD_X, LV_PART_MAIN);
    lv_obj_set_style_pad_top(obj, MSG_PAD_Y, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(obj, MSG_PAD_Y, LV_PART_MAIN);

    lv_obj_set_style_text_font(obj, FONT_CN, LV_PART_MAIN);
    lv_obj_set_style_text_color(obj, hex(textColor), LV_PART_MAIN);
    lv_obj_set_style_text_opa(obj, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_text_align(obj, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(obj, MSG_LINE_SPACE, LV_PART_MAIN);

    lv_obj_set_style_border_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(obj, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(obj, 0, LV_PART_MAIN);
    lv_obj_clear_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    return obj;
}

static uint32_t imeSelectedBgColor(ImeCandidateColorRole role)
{
    switch (role) {
        case IME_COLOR_PINYIN: return C_IME_PINYIN_SEL_BG;
        case IME_COLOR_HANZI:  return C_IME_HANZI_SEL_BG;
        case IME_COLOR_EN_NUM: return C_IME_EN_NUM_SEL_BG;
        default:               return C_IME_EN_NUM_SEL_BG;
    }
}

static uint8_t imeUtf8CharBytes(uint8_t firstByte)
{
    if ((firstByte & 0x80) == 0x00) return 1;
    if ((firstByte & 0xE0) == 0xC0) return 2;
    if ((firstByte & 0xF0) == 0xE0) return 3;
    if ((firstByte & 0xF8) == 0xF0) return 4;
    return 1;
}

static int16_t imeCandidateTextWidth(const String& text)
{
    int16_t w = 10;
    const char *p = text.c_str();

    while (*p) {
        uint8_t b = (uint8_t)*p;
        if ((b & 0x80) == 0x00) {
            w += 8;
            p++;
        } else {
            w += 16;
            p += imeUtf8CharBytes(b);
        }
    }

    if (w < 20) w = 20;
    return w;
}

static void hideImeCandidateSlots(lv_obj_t **boxes)
{
    for (uint8_t i = 0; i < IME_MAX_ROW_ITEMS; i++) {
        if (boxes[i]) {
            lv_obj_add_flag(boxes[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void createImeCandidateSlots(lv_obj_t *parent,
                                    lv_obj_t **boxes,
                                    lv_obj_t **labels)
{
    for (uint8_t i = 0; i < IME_MAX_ROW_ITEMS; i++) {
        boxes[i] = box(parent, 0, 0, 20, IME_ROW_H, C_ROW_BG, 3);
        lv_obj_set_style_bg_opa(boxes[i], LV_OPA_TRANSP, LV_PART_MAIN);

        labels[i] = label(boxes[i], "", 3, 0, 14, IME_ROW_H,
                          FONT_CN, C_TEXT, LV_TEXT_ALIGN_CENTER);

        lv_obj_add_flag(boxes[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static lv_obj_t *imePlainLabelForRow(ImeCandidateRow row)
{
    return (row == IME_ROW_BOTTOM) ? g_hanziLabel : g_pinyinLabel;
}

static lv_obj_t **imeCandidateBoxesForRow(ImeCandidateRow row)
{
    return (row == IME_ROW_BOTTOM) ? g_hanziCandBox : g_pinyinCandBox;
}

static lv_obj_t **imeCandidateLabelsForRow(ImeCandidateRow row)
{
    return (row == IME_ROW_BOTTOM) ? g_hanziCandLabel : g_pinyinCandLabel;
}

static lv_obj_t *circleText(lv_obj_t *parent,
                            int16_t x,
                            int16_t y,
                            int16_t size,
                            uint32_t bgColor,
                            const char *txt)
{
    lv_obj_t *c = box(parent, x, y, size, size, bgColor, LV_RADIUS_CIRCLE);
    centerLabel(c, txt, FONT_CN, C_TEXT);
    return c;
}

// =====================
// 顶部图标
// =====================
static void makeMsgIcon(lv_obj_t *parent, int16_t x, int16_t y)
{
    g_mqttParts[0] = box(parent, x, y, 19, 13, C_WHITE, 4);
    g_mqttParts[1] = box(g_mqttParts[0], 4, 5, 2, 2, C_TOP_BAR, LV_RADIUS_CIRCLE);
    g_mqttParts[2] = box(g_mqttParts[0], 8, 5, 2, 2, C_TOP_BAR, LV_RADIUS_CIRCLE);
    g_mqttParts[3] = box(g_mqttParts[0], 12, 5, 2, 2, C_TOP_BAR, LV_RADIUS_CIRCLE);
    g_mqttParts[4] = box(parent, x + 4, y + 11, 5, 5, C_WHITE, 1);
}

static void makeBattery(lv_obj_t *parent, int16_t x, int16_t y)
{
    borderBox(parent, x, y, 25, 15, C_WHITE, 2, 3);
    box(parent, x + 25, y + 4, 3, 7, C_WHITE, 1);
    g_batteryFill = box(parent, x + 4, y + 4, 16, 7, C_GREEN, 1);
    // 数字放在电池框里；不显示 %，避免 100% 在小图标里挤压。
    g_batteryLabel = label(parent, "--", x, y + 2, 25, 12,
                           FONT_BATT, C_WHITE, LV_TEXT_ALIGN_CENTER);
}

static void makeEmoji(lv_obj_t *parent, int16_t x, int16_t y)
{
    lv_obj_t *face = borderBox(parent, x, y, 17, 17, C_TEXT, 1, LV_RADIUS_CIRCLE);
    box(face, 4, 5, 2, 2, C_TEXT, LV_RADIUS_CIRCLE);
    box(face, 11, 5, 2, 2, C_TEXT, LV_RADIUS_CIRCLE);
    box(face, 5, 11, 7, 1, C_TEXT, 0);
}

// =====================
// 聊天消息绘制
// =====================
static int16_t clamp16(int16_t v, int16_t lo, int16_t hi)
{
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

static void measureBubble(const char *text,
                          int16_t &bubbleW,
                          int16_t &bubbleH,
                          int16_t &labelW,
                          int16_t &labelH)
{
    if (text == nullptr) text = "";

    lv_point_t oneLine;
    lv_txt_get_size(&oneLine,
                    text,
                    FONT_CN,
                    0,
                    MSG_LINE_SPACE,
                    1000,
                    LV_TEXT_FLAG_NONE);

    labelW = clamp16(oneLine.x, MSG_MIN_TEXT_W, MSG_MAX_TEXT_W);

    lv_point_t wrapped;
    lv_txt_get_size(&wrapped,
                    text,
                    FONT_CN,
                    0,
                    MSG_LINE_SPACE,
                    labelW,
                    LV_TEXT_FLAG_NONE);

    const int16_t lineH = lv_font_get_line_height(FONT_CN);
    labelH = wrapped.y;
    if (labelH < lineH) labelH = lineH;

    bubbleW = labelW + MSG_PAD_X * 2;
    bubbleH = labelH + MSG_PAD_Y * 2;

    if (bubbleW < MSG_MIN_BUBBLE_W) bubbleW = MSG_MIN_BUBBLE_W;
    if (bubbleH < MSG_MIN_BUBBLE_H) bubbleH = MSG_MIN_BUBBLE_H;
}

static int16_t bubbleHeightForText(const char *text)
{
    int16_t bw, bh, lw, lh;
    measureBubble(text, bw, bh, lw, lh);
    return bh;
}

static bool sameLocalDay(uint32_t a, uint32_t b)
{
    if (a == 0 || b == 0) return false;

    time_t ta = (time_t)a;
    time_t tb = (time_t)b;
    struct tm la;
    struct tm lb;

    if (!localtime_r(&ta, &la) || !localtime_r(&tb, &lb)) {
        return false;
    }

    return la.tm_year == lb.tm_year && la.tm_yday == lb.tm_yday;
}

static bool shouldShowTimeDivider(uint8_t idx)
{
    if (idx >= g_msgCount || g_msgTs[idx] == 0) return false;

    if (idx == 0) return true;

    const uint32_t nowTs = g_msgTs[idx];
    const uint32_t prevTs = g_msgTs[idx - 1];
    if (prevTs == 0) return true;

    if (!sameLocalDay(nowTs, prevTs)) return true;

    return (nowTs > prevTs) && ((nowTs - prevTs) >= CHAT_TIME_GAP_SEC);
}

static void formatTimeDivider(uint8_t idx, char *buf, size_t len)
{
    if (!buf || len == 0) return;
    buf[0] = '\0';

    if (idx >= g_msgCount || g_msgTs[idx] == 0) return;

    time_t t = (time_t)g_msgTs[idx];
    struct tm localTime;
    if (!localtime_r(&t, &localTime)) return;

    const uint32_t nowTs = (uint32_t)time(nullptr);
    const bool firstMessageFromToday = (idx == 0) && sameLocalDay(g_msgTs[idx], nowTs);

    if (idx != 0 && sameLocalDay(g_msgTs[idx], g_msgTs[idx - 1])) {
        snprintf(buf, len, "%02d:%02d",
                 localTime.tm_hour,
                 localTime.tm_min);
    } else if (firstMessageFromToday) {
        snprintf(buf, len, "%02d:%02d",
                 localTime.tm_hour,
                 localTime.tm_min);
    } else {
        snprintf(buf, len, "%02d-%02d %02d:%02d",
                 localTime.tm_mon + 1,
                 localTime.tm_mday,
                 localTime.tm_hour,
                 localTime.tm_min);
    }
}

static void drawTimeDivider(uint8_t idx, int16_t y)
{
    char buf[16];
    formatTimeDivider(idx, buf, sizeof(buf));
    if (buf[0] == '\0') return;

    label(g_chatPanel, buf, 0, y, 232, TIME_LABEL_H,
          FONT_CN, C_TIME_TEXT, LV_TEXT_ALIGN_CENTER);
}

static void setMessageViewToBottom()
{
    g_msgAutoFollow = true;
    g_msgViewLast = (g_msgCount > 0) ? (int16_t)(g_msgCount - 1) : -1;
}

static void clampMessageView()
{
    if (g_msgCount == 0) {
        g_msgViewLast = -1;
        g_msgAutoFollow = true;
        return;
    }

    if (g_msgAutoFollow || g_msgViewLast < 0) {
        g_msgViewLast = (int16_t)(g_msgCount - 1);
    } else if (g_msgViewLast >= g_msgCount) {
        g_msgViewLast = (int16_t)(g_msgCount - 1);
    }
}

static bool shouldShowTimeDividerInWindow(uint8_t idx, uint8_t windowFirst)
{
    if (idx >= g_msgCount || g_msgTs[idx] == 0) return false;
    return idx == windowFirst || shouldShowTimeDivider(idx);
}

static int32_t renderedMessageHeight(uint8_t idx, uint8_t windowFirst)
{
    int32_t h = 0;
    if (shouldShowTimeDividerInWindow(idx, windowFirst)) {
        h += TIME_LABEL_H + TIME_AFTER_GAP;
    }

    h += bubbleHeightForText(g_msgText[idx].c_str()) + CHAT_GAP;
    return h;
}

static int32_t renderedWindowHeight(uint8_t first, uint8_t last)
{
    int32_t h = CHAT_TOP_PAD + CHAT_BOTTOM_PAD;
    for (uint8_t i = first; i <= last; i++) {
        h += renderedMessageHeight(i, first);
        if (i == last) break;
    }
    return h;
}

static uint8_t findWindowFirst(uint8_t last)
{
    uint8_t best = last;

    for (int16_t first = last; first >= 0; first--) {
        int32_t h = renderedWindowHeight((uint8_t)first, last);
        if (h > CHAT_PANEL_H && first < last) {
            return (uint8_t)(first + 1);
        }

        best = (uint8_t)first;
        if (first == 0) break;
    }

    return best;
}

static bool scrollRenderedWindow(int8_t direction)
{
    if (!g_chatPanel || direction == 0) return false;

    lv_obj_update_layout(g_chatPanel);

    if (direction < 0 && lv_obj_get_scroll_top(g_chatPanel) > 0) {
        lv_obj_scroll_by_bounded(g_chatPanel, 0, CHAT_SCROLL_STEP, LV_ANIM_ON);
        g_msgAutoFollow = false;
        return true;
    }

    if (direction > 0 && lv_obj_get_scroll_bottom(g_chatPanel) > 0) {
        lv_obj_scroll_by_bounded(g_chatPanel, 0, -CHAT_SCROLL_STEP, LV_ANIM_ON);
        return true;
    }

    return false;
}

static void drawOneMessage(uint8_t idx, int16_t y)
{
    const bool mine = g_msgMine[idx];

    int16_t bubbleW, bubbleH, labelW, labelH;
    measureBubble(g_msgText[idx].c_str(), bubbleW, bubbleH, labelW, labelH);

    // 历史同步可能一次显示近 100 条消息；每条消息只创建一个 LVGL label，
    // 避免头像/容器/文本多对象叠加导致 LVGL 内存池耗尽。
    if (mine) {
        const int16_t bubbleX = 226 - bubbleW;
        bubbleLabel(g_chatPanel,
                    g_msgText[idx].c_str(),
                    bubbleX,
                    y,
                    bubbleW,
                    bubbleH,
                    C_BUBBLE_RIGHT,
                    C_TEXT);
    } else {
        bubbleLabel(g_chatPanel,
                    g_msgText[idx].c_str(),
                    6,
                    y,
                    bubbleW,
                    bubbleH,
                    C_BUBBLE_LEFT,
                    C_WHITE);
    }
    return;

    const int16_t avatarSize = 22;
    const int16_t avatarY = y + (bubbleH - avatarSize) / 2;


    //目前的显示效果中并没有"易" 和 "曦"，但是我觉得这个效果更好。空间更足
    if (mine) {
        circleText(g_chatPanel, 202, avatarY, avatarSize, C_BUBBLE_RIGHT, "易");

        const int16_t bubbleX = 202 - 6 - bubbleW;
        lv_obj_t *bubble = box(g_chatPanel, bubbleX, y, bubbleW, bubbleH, C_BUBBLE_RIGHT, 8);

        lv_obj_t *txt = label(bubble, g_msgText[idx].c_str(),
                              MSG_PAD_X, MSG_PAD_Y,
                              labelW, labelH,
                              FONT_CN, C_TEXT, LV_TEXT_ALIGN_LEFT);
        lv_label_set_long_mode(txt, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_line_space(txt, MSG_LINE_SPACE, LV_PART_MAIN);
    } else {
        circleText(g_chatPanel, 10, avatarY, avatarSize, C_AVATAR_PURPLE, "曦");

        lv_obj_t *bubble = box(g_chatPanel, 42, y, bubbleW, bubbleH, C_BUBBLE_LEFT, 8);

        lv_obj_t *txt = label(bubble, g_msgText[idx].c_str(),
                              MSG_PAD_X, MSG_PAD_Y,
                              labelW, labelH,
                              FONT_CN, C_WHITE, LV_TEXT_ALIGN_LEFT);
        lv_label_set_long_mode(txt, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_line_space(txt, MSG_LINE_SPACE, LV_PART_MAIN);
    }
}

static void redrawChatMessages()
{
    if (!g_chatPanel) return;
    if (!ensureMessageStorage()) return;

    lv_obj_clean(g_chatPanel);
    lv_obj_scroll_to_y(g_chatPanel, 0, LV_ANIM_OFF);

    clampMessageView();
    if (g_msgViewLast < 0) return;

    const uint8_t last = (uint8_t)g_msgViewLast;
    const uint8_t first = findWindowFirst(last);
    const int32_t totalH = renderedWindowHeight(first, last);

    int16_t y = CHAT_TOP_PAD;
    if (totalH < CHAT_PANEL_H) {
        y = (int16_t)(CHAT_PANEL_H - totalH + CHAT_TOP_PAD);
    }

    for (uint8_t i = first; i <= last; i++) {
        if (shouldShowTimeDividerInWindow(i, first)) {
            drawTimeDivider(i, y);
            y += TIME_LABEL_H + TIME_AFTER_GAP;
        }

        int16_t h = bubbleHeightForText(g_msgText[i].c_str());
        drawOneMessage(i, y);
        y += h + CHAT_GAP;

        if ((i & 0x03) == 0) yield();
        if (i == last) break;
    }
}

// =====================
// UI 分区
// =====================
static void createTopBar()
{
    lv_obj_t *top = box(g_root, 0, 0, 240, 30, C_TOP_BAR, 0);

    lv_obj_t *avatar = box(top, 2, 2, 26, 26, C_WHITE, LV_RADIUS_CIRCLE);
    centerLabel(avatar, "猫", FONT_CN, C_TEXT);

    // 头像右侧放两行时间，不挤占右侧 mode / WiFi / 电池图标。
    g_dateLabel = label(top, "---- -- --", 34, 2, 78, 12,
                        FONT_CN, C_WHITE, LV_TEXT_ALIGN_LEFT);
    g_timeLabel = label(top, "--- --:--", 34, 15, 78, 12,
                        FONT_CN, C_WHITE, LV_TEXT_ALIGN_LEFT);

    // 给 mode 留足宽度，num 不会再因为宽度不够导致 m 残影。
    // label 自带不透明背景，切换 cn/en/num 时会把旧字符区域完整擦掉。
    g_modeLabel = label(top, "cn", 116, 6, 38, 18,
                        FONT_CN, C_GREEN, LV_TEXT_ALIGN_CENTER);
    lv_obj_set_style_bg_color(g_modeLabel, hex(C_TOP_BAR), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_modeLabel, LV_OPA_COVER, LV_PART_MAIN);

    // 整体向右挪一点，避免 mode 和 MQTT 图标挤在一起。
    makeMsgIcon(top, 156, 8);

    g_wifiLabel = label(top, LV_SYMBOL_WIFI, 181, 5, 22, 20,
                        FONT_SMALL, C_GREEN, LV_TEXT_ALIGN_CENTER);

    makeBattery(top, 208, 8);
}

static void createChatArea()
{
    g_chatPanel = box(g_root, 4, 36, 232, CHAT_PANEL_H, C_CHAT_PANEL, 7);
    lv_obj_add_flag(g_chatPanel, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scroll_dir(g_chatPanel, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_chatPanel, LV_SCROLLBAR_MODE_OFF);
    redrawChatMessages();
}

static void createImeArea()
{
    // 拼音区：247 ~ 261
    g_pinyinRow = box(g_root, 0, 247, 240, IME_ROW_H, C_ROW_BG, 2);
    g_pinyinLabel = label(g_pinyinRow, "", 20, 0, 210, IME_ROW_H,
                          FONT_CN, C_TEXT, LV_TEXT_ALIGN_LEFT);
    createImeCandidateSlots(g_pinyinRow, g_pinyinCandBox, g_pinyinCandLabel);

    // 分隔线：263
    box(g_root, 0, 263, 240, 1, C_SCREEN_BG, 0);

    // 汉字候选区：265 ~ 279
    g_hanziRow = box(g_root, 0, 265, 240, IME_ROW_H, C_ROW_BG, 2);
    g_hanziLabel = label(g_hanziRow, "", 20, 0, 210, IME_ROW_H,
                         FONT_CN, C_TEXT, LV_TEXT_ALIGN_LEFT);
    createImeCandidateSlots(g_hanziRow, g_hanziCandBox, g_hanziCandLabel);
}

static void createBottomBar()
{
    lv_obj_t *bottom = box(g_root, 0, 284, 240, 36, C_BOTTOM_BG, 0);

    makeEmoji(bottom, 7, 9);

    g_inputTa = lv_textarea_create(bottom);
    lv_obj_remove_style_all(g_inputTa);

    lv_obj_set_pos(g_inputTa, 30, 2);
    lv_obj_set_size(g_inputTa, 154, 32);

    lv_obj_set_style_bg_color(g_inputTa, hex(C_WHITE), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_inputTa, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_set_style_radius(g_inputTa, 9, LV_PART_MAIN);

    lv_obj_set_style_border_width(g_inputTa, 0, LV_PART_MAIN);
    lv_obj_set_style_outline_width(g_inputTa, 0, LV_PART_MAIN);
    lv_obj_set_style_shadow_width(g_inputTa, 0, LV_PART_MAIN);

    lv_obj_set_style_text_font(g_inputTa, FONT_CN, LV_PART_MAIN);
    lv_obj_set_style_text_color(g_inputTa, hex(C_TEXT), LV_PART_MAIN);
    lv_obj_set_style_text_opa(g_inputTa, LV_OPA_COVER, LV_PART_MAIN);

    lv_obj_set_style_pad_left(g_inputTa, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_right(g_inputTa, 5, LV_PART_MAIN);
    lv_obj_set_style_pad_top(g_inputTa, 2, LV_PART_MAIN);
    lv_obj_set_style_pad_bottom(g_inputTa, 1, LV_PART_MAIN);
    lv_obj_set_style_text_line_space(g_inputTa, 1, LV_PART_MAIN);

    lv_textarea_set_one_line(g_inputTa, false);
    lv_obj_set_scroll_dir(g_inputTa, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(g_inputTa, LV_SCROLLBAR_MODE_OFF);
    lv_textarea_set_max_length(g_inputTa, 0);
    lv_textarea_set_text(g_inputTa, "");

    // remove_style_all() also removes the default textarea cursor style.
    lv_obj_set_style_border_color(g_inputTa, hex(C_TEXT), LV_PART_CURSOR);
    lv_obj_set_style_border_width(g_inputTa, 2, LV_PART_CURSOR);
    lv_obj_set_style_border_side(g_inputTa, LV_BORDER_SIDE_LEFT, LV_PART_CURSOR);
    lv_obj_set_style_bg_opa(g_inputTa, LV_OPA_TRANSP, LV_PART_CURSOR);
    lv_obj_set_style_anim_time(g_inputTa, 0, LV_PART_CURSOR);
    lv_textarea_set_cursor_pos(g_inputTa, 0);

    lv_obj_t *send = box(bottom, 190, 3, 47, 30, C_SEND_BTN, 8);
    centerLabel(send, "发送", FONT_CN, C_WHITE);
}

static void scrollInputCursorIntoView()
{
    if (!g_inputTa) return;

    lv_obj_update_layout(g_inputTa);

    lv_obj_t *taLabel = lv_textarea_get_label(g_inputTa);
    if (!taLabel) return;

    lv_point_t cursorPoint;
    lv_label_get_letter_pos(taLabel,
                            lv_textarea_get_cursor_pos(g_inputTa),
                            &cursorPoint);

    const lv_font_t *font = lv_obj_get_style_text_font(g_inputTa, LV_PART_MAIN);
    const lv_coord_t lineH = lv_font_get_line_height(font);

    lv_coord_t targetX = lv_obj_get_scroll_left(g_inputTa);
    const lv_coord_t contentW = lv_obj_get_content_width(g_inputTa);
    if (cursorPoint.x < targetX) {
        targetX = cursorPoint.x;
    } else if (cursorPoint.x + lineH > targetX + contentW) {
        targetX = cursorPoint.x - contentW + lineH;
    }
    if (targetX < 0) targetX = 0;

    lv_coord_t targetY = lv_obj_get_scroll_top(g_inputTa);
    const lv_coord_t contentH = lv_obj_get_content_height(g_inputTa);
    if (cursorPoint.y < targetY) {
        targetY = cursorPoint.y;
    } else if (cursorPoint.y + lineH > targetY + contentH) {
        targetY = cursorPoint.y - contentH + lineH;
    }
    if (targetY < 0) targetY = 0;

    lv_obj_scroll_to(g_inputTa, targetX, targetY, LV_ANIM_OFF);
}

// =====================
// 对外接口
// =====================
void ui_chat_create()
{
    g_root = lv_scr_act();

    // 只在 setup 创建一次；这里清旧对象是为了避免旧 UI 残留。
    lv_obj_clean(g_root);

    lv_obj_set_style_bg_color(g_root, hex(C_SCREEN_BG), LV_PART_MAIN);
    lv_obj_set_style_bg_opa(g_root, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_clear_flag(g_root, LV_OBJ_FLAG_SCROLLABLE);

    g_msgCount = 0;
    if (ensureMessageStorage()) {
        for (uint8_t i = 0; i < CHAT_MAX; i++) {
            g_msgText[i] = "";
            g_msgMine[i] = false;
            g_msgTs[i] = 0;
        }
    }

    createTopBar();
    createChatArea();
    createImeArea();
    createBottomBar();

    ui_chat_setMode("cn");
    ui_chat_setWifi(false);
    ui_chat_setMqtt(false);
    ui_chat_setBattery(0);
    ui_chat_setIme("", "");
    ui_chat_setInputText("", 0);
}

void ui_chat_setMode(const char *mode)
{
    if (!g_modeLabel) return;

    const char *m = "cn";
    if (mode) {
        if (strcmp(mode, "中文") == 0 || strcmp(mode, "cn") == 0 || strcmp(mode, "CN") == 0) {
            m = "cn";
        } else if (strcmp(mode, "英文") == 0 || strcmp(mode, "en") == 0 || strcmp(mode, "EN") == 0) {
            m = "en";
        } else if (strcmp(mode, "数字") == 0 || strcmp(mode, "num") == 0 || strcmp(mode, "NUM") == 0) {
            m = "num";
        } else {
            m = mode;
        }
    }

    lv_label_set_text(g_modeLabel, m);
}

void ui_chat_setClock(const char *dateText, const char *timeText)
{
    if (g_dateLabel) {
        lv_label_set_text(g_dateLabel, dateText ? dateText : "");
    }

    if (g_timeLabel) {
        lv_label_set_text(g_timeLabel, timeText ? timeText : "");
    }
}

void ui_chat_setWifi(bool connected)
{
    if (!g_wifiLabel) return;
    lv_obj_set_style_text_color(g_wifiLabel,
                                hex(connected ? C_GREEN : C_GRAY),
                                LV_PART_MAIN);
}

void ui_chat_setMqtt(bool connected)
{
    // MQTT 图标本体保留。连接成功才显示中间 3 个小点；未连接时隐藏 3 个小点。
    // 这样不会出现“灰色小点”误判，也和你图里 WiFi 左侧图标逻辑一致。
    setBg(g_mqttParts[0], C_WHITE);
    setBg(g_mqttParts[4], C_WHITE);

    for (uint8_t i = 1; i <= 3; i++) {
        if (!g_mqttParts[i]) continue;

        if (connected) {
            lv_obj_clear_flag(g_mqttParts[i], LV_OBJ_FLAG_HIDDEN);
            setBg(g_mqttParts[i], C_TOP_BAR);
        } else {
            lv_obj_add_flag(g_mqttParts[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

void ui_chat_setBattery(uint8_t percent)
{
    if (!g_batteryFill) return;

    if (percent > 100) percent = 100;

    int16_t w = (int16_t)((16 * percent) / 100);
    if (percent > 0 && w < 1) w = 1;

    setBg(g_batteryFill, (percent < 20) ? C_BATTERY_LOW : C_GREEN);
    lv_obj_set_width(g_batteryFill, w);

    if (g_batteryLabel) {
        char buf[4];
        snprintf(buf, sizeof(buf), "%u", percent);
        lv_label_set_text(g_batteryLabel, buf);
        // 电量条覆盖到数字区域时用深色字；低电量/空白背景上保留白色字。
        lv_obj_set_style_text_color(g_batteryLabel,
                                    hex((percent >= 55) ? C_BATTERY_TEXT_ON_FILL : C_WHITE),
                                    LV_PART_MAIN);
        lv_obj_move_foreground(g_batteryLabel);
    }
}

void ui_chat_setIme(const char *pinyin, const char *hanzi)
{
    ui_chat_setImeRowText(IME_ROW_TOP, pinyin);
    ui_chat_setImeRowText(IME_ROW_BOTTOM, hanzi);
}

void ui_chat_setImeRowText(ImeCandidateRow row, const char *text)
{
    lv_obj_t *plain = imePlainLabelForRow(row);
    lv_obj_t **boxes = imeCandidateBoxesForRow(row);

    hideImeCandidateSlots(boxes);

    if (plain) {
        lv_obj_clear_flag(plain, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(plain, text ? text : "");
    }
}

void ui_chat_setImeCandidates(ImeCandidateRow row,
                              const String *items,
                              uint8_t count,
                              uint8_t selected,
                              uint8_t maxShow,
                              ImeCandidateColorRole colorRole)
{
    if (items == nullptr || count == 0 || maxShow == 0) {
        ui_chat_setImeRowText(row, "");
        return;
    }

    lv_obj_t *plain = imePlainLabelForRow(row);
    lv_obj_t **boxes = imeCandidateBoxesForRow(row);
    lv_obj_t **labels = imeCandidateLabelsForRow(row);

    if (plain) {
        lv_obj_add_flag(plain, LV_OBJ_FLAG_HIDDEN);
    }
    hideImeCandidateSlots(boxes);

    if (selected >= count) selected = 0;
    if (maxShow > IME_MAX_ROW_ITEMS) maxShow = IME_MAX_ROW_ITEMS;

    uint8_t page = selected / maxShow;
    uint8_t start = page * maxShow;
    uint8_t end = start + maxShow;
    if (end > count) end = count;

    uint8_t visibleCount = end - start;
    if (visibleCount == 0) return;

    const int16_t rowX = 20;
    const int16_t rowW = 210;
    const int16_t gap = 4;
    const int16_t rowH = IME_ROW_H;
    int16_t maxSlotW = (rowW - (gap * (visibleCount - 1))) / visibleCount;
    if (maxSlotW < 20) maxSlotW = 20;

    int16_t x = rowX;
    uint32_t selectedBg = imeSelectedBgColor(colorRole);

    for (uint8_t slot = 0; slot < visibleCount; slot++) {
        uint8_t itemIndex = start + slot;
        bool isSelected = (itemIndex == selected);

        int16_t w = imeCandidateTextWidth(items[itemIndex]);
        if (w > maxSlotW) w = maxSlotW;

        lv_obj_set_pos(boxes[slot], x, 0);
        lv_obj_set_size(boxes[slot], w, rowH);
        lv_obj_clear_flag(boxes[slot], LV_OBJ_FLAG_HIDDEN);

        lv_label_set_text(labels[slot], items[itemIndex].c_str());
        lv_obj_set_pos(labels[slot], 3, 0);
        lv_obj_set_size(labels[slot], (w > 6) ? (w - 6) : w, rowH);

        if (isSelected) {
            lv_obj_set_style_bg_color(boxes[slot], hex(selectedBg), LV_PART_MAIN);
            lv_obj_set_style_bg_opa(boxes[slot], LV_OPA_COVER, LV_PART_MAIN);
            lv_obj_set_style_text_color(labels[slot], hex(C_IME_SEL_TEXT), LV_PART_MAIN);
        } else {
            lv_obj_set_style_bg_opa(boxes[slot], LV_OPA_TRANSP, LV_PART_MAIN);
            lv_obj_set_style_text_color(labels[slot], hex(C_TEXT), LV_PART_MAIN);
        }

        x += w + gap;
    }
}

void ui_chat_setInputText(const char *text, uint16_t cursorPos)
{
    if (!g_inputTa) return;

    const char *safeText = text ? text : "";
    if (strcmp(lv_textarea_get_text(g_inputTa), safeText) != 0) {
        lv_textarea_set_text(g_inputTa, safeText);
    }

    lv_textarea_set_cursor_pos(g_inputTa, cursorPos);
    scrollInputCursorIntoView();
}

const char *ui_chat_getInputText()
{
    if (!g_inputTa) return "";
    return lv_textarea_get_text(g_inputTa);
}

void ui_chat_beginMessageBatch()
{
    g_msgBatchMode = true;
    g_msgBatchDirty = false;
}

void ui_chat_endMessageBatch(bool scrollToBottom)
{
    if (!g_msgBatchMode) return;

    const bool dirty = g_msgBatchDirty;
    g_msgBatchMode = false;
    g_msgBatchDirty = false;

    if (!dirty) return;

    if (scrollToBottom) {
        setMessageViewToBottom();
    } else {
        clampMessageView();
    }

    redrawChatMessages();
    if (scrollToBottom && g_chatPanel) {
        lv_obj_scroll_to_y(g_chatPanel, LV_COORD_MAX, LV_ANIM_OFF);
    }
}

void ui_chat_addMessage(const char *text, bool mine, uint32_t ts)
{
    if (text == nullptr || text[0] == '\0') return;
    if (!ensureMessageStorage()) return;

    const bool wasAutoFollow = g_msgAutoFollow;

    if (g_msgCount < CHAT_MAX) {
        g_msgText[g_msgCount] = text;
        g_msgMine[g_msgCount] = mine;
        g_msgTs[g_msgCount] = ts;
        g_msgCount++;
    } else {
        for (uint8_t i = 1; i < CHAT_MAX; i++) {
            g_msgText[i - 1] = g_msgText[i];
            g_msgMine[i - 1] = g_msgMine[i];
            g_msgTs[i - 1] = g_msgTs[i];
        }
        g_msgText[CHAT_MAX - 1] = text;
        g_msgMine[CHAT_MAX - 1] = mine;
        g_msgTs[CHAT_MAX - 1] = ts;

        if (!wasAutoFollow && g_msgViewLast > 0) {
            g_msgViewLast--;
        }
    }

    if (wasAutoFollow) {
        setMessageViewToBottom();
    } else {
        clampMessageView();
    }

    if (g_msgBatchMode) {
        g_msgBatchDirty = true;
        return;
    }

    redrawChatMessages();
    if (wasAutoFollow && g_chatPanel) {
        lv_obj_scroll_to_y(g_chatPanel, LV_COORD_MAX, LV_ANIM_OFF);
    }
}

void ui_chat_clearMessages()
{
    g_msgCount = 0;
    setMessageViewToBottom();

    if (ensureMessageStorage()) {
        for (uint8_t i = 0; i < CHAT_MAX; i++) {
            g_msgText[i] = "";
            g_msgMine[i] = false;
            g_msgTs[i] = 0;
        }
    }

    if (g_msgBatchMode) {
        g_msgBatchDirty = true;
        return;
    }

    redrawChatMessages();
    if (g_chatPanel) {
        lv_obj_scroll_to_y(g_chatPanel, 0, LV_ANIM_OFF);
    }
}

void ui_chat_scrollMessages(int8_t direction)
{
    if (!g_chatPanel || direction == 0 || g_msgCount == 0) return;

    if (scrollRenderedWindow(direction)) return;

    clampMessageView();
    if (g_msgViewLast < 0) return;

    const uint8_t last = (uint8_t)g_msgViewLast;
    const uint8_t first = findWindowFirst(last);

    if (direction < 0) {
        if (first == 0) return;
        g_msgAutoFollow = false;
        g_msgViewLast = (int16_t)(first - 1);
    } else {
        if (last >= g_msgCount - 1) {
            setMessageViewToBottom();
        } else {
            uint8_t step = last - first + 1;
            if (step == 0) step = 1;

            uint16_t nextLast = (uint16_t)last + step;
            if (nextLast >= g_msgCount - 1) {
                setMessageViewToBottom();
            } else {
                g_msgAutoFollow = false;
                g_msgViewLast = (int16_t)nextLast;
            }
        }
    }

    redrawChatMessages();
    if (g_msgAutoFollow && g_chatPanel) {
        lv_obj_scroll_to_y(g_chatPanel, LV_COORD_MAX, LV_ANIM_OFF);
    }
}
