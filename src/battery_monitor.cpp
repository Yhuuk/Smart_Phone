#include "battery_monitor.h"

// =====================
// 硬件连接
// =====================
//
// 原理图中的网络和引脚：
// - DETECT_CONTROL(P13) -> ESP32 GPIO13，用开漏输出打开/关闭电池分压检测电路。
// - BATTERY_DETECT(P33) -> ESP32 GPIO33，也就是 ADC1_CH5，用来读取分压电压。
//
// GPIO33 属于 ADC1，ESP32 在 WiFi 工作时仍然可以稳定使用；不要换成 ADC2
// 引脚，否则 WiFi 开启后 ADC 读取可能失败或不稳定。
static constexpr uint8_t DETECT_CONTROL_PIN = 13;
static constexpr uint8_t BATTERY_DETECT_PIN = 33;

// Q5 控制脚由 R26 上拉关断，ESP32 只负责开漏下拉开启。
// - LOW  ：GPIO13 下拉，Q5 开启，允许测量；
// - HIGH ：开漏输出释放为高阻态，由 R26 上拉，Q5 关断。
static constexpr uint8_t DETECT_ENABLE_LEVEL = LOW;
static constexpr uint8_t DETECT_DISABLE_LEVEL = HIGH;

// =====================
// 分压与 ADC 参数
// =====================
//
// 从原理图文本层读取到：
// - R24 = 1.5K
// - R25 = 3.9K
//
// 按当前电路理解，分压关系为：
//   VBAT -> R24 -> BATTERY_DETECT -> R25 -> 由 DETECT_CONTROL 控制到地
//
// 因此：
//   Vadc = Vbat * R25 / (R24 + R25)
//   Vbat = Vadc * (R24 + R25) / R25
//
// 4.2V 满电时，ADC 端约为 4.2 * 3.9 / 5.4 = 3.03V，适合 ESP32 11dB
// 衰减量程。若你实测发现电压计算明显反了，优先核对 R24/R25 在原理图中
// 谁接 VBAT、谁接控制端，然后调整下面两个常量。
static constexpr float R_TOP_OHMS = 1500.0f;
static constexpr float R_BOTTOM_OHMS = 3900.0f;
static constexpr float DIVIDER_RESTORE_RATIO =
    (R_TOP_OHMS + R_BOTTOM_OHMS) / R_BOTTOM_OHMS;

// ADC 和电阻本身都有误差。如果用万用表量到的真实电池电压与串口打印的
// battery_monitor_voltage() 有固定比例偏差，可以微调这个系数。
// 例如显示 4.10V、实测 4.20V，则可设为 4.20 / 4.10 = 1.024。
static constexpr float VOLTAGE_CALIBRATION = 1.0f;

// =====================
// 采样策略
// =====================
//
// 每分钟测一次已经足够。锂电池电压变化很慢，频繁测量只会增加分压电阻
// 导通时间和 ADC 噪声刷新频率。
static constexpr uint32_t MEASURE_INTERVAL_MS = 60000;

// 拉低 DETECT_CONTROL 开启 Q5 后，分压节点需要一点时间稳定。20ms 对 1.5K/3.9K
// 这种低阻值分压来说很宽裕，同时对 UI 影响很小。
static constexpr uint16_t DETECT_SETTLE_MS = 20;

// 多次采样取平均，减少 ESP32 ADC 的瞬时噪声。16 次采样每分钟才发生一次，
// 总耗时仍然很短。
static constexpr uint8_t ADC_SAMPLE_COUNT = 16;
static constexpr uint8_t ADC_SAMPLE_DELAY_MS = 2;

// 简单低通滤波，避免百分比在相邻档位之间跳动。
// 第一次测量直接采用原始值；之后使用 75% 旧值 + 25% 新值。
static constexpr uint8_t FILTER_OLD_WEIGHT = 3;
static constexpr uint8_t FILTER_NEW_WEIGHT = 1;

struct BatteryCurvePoint {
    uint16_t voltageMv;
    uint8_t percent;
};

// 单节锂电池开路电压到剩余容量的近似曲线。
//
// 注意：这是用于“显示百分比”的经验曲线，不是精密电量计。
// 真实电量会受电池型号、负载电流、温度、老化程度影响。这里用查表 +
// 区间线性插值，让 UI 仍然能显示 0~100 的连续数字，比简单 3.3~4.2
// 线性映射更接近锂电池实际放电平台。
static const BatteryCurvePoint BATTERY_CURVE[] = {
    {4200, 100},
    {4100, 90},
    {4000, 80},
    {3920, 70},
    {3850, 60},
    {3790, 50},
    {3730, 40},
    {3680, 30},
    {3620, 20},
    {3550, 10},
    {3450, 5},
    {3300, 0},
};

static bool s_hasReading = false;
static uint32_t s_lastMeasureMs = 0;
static uint16_t s_batteryVoltageMv = 0;
static uint8_t s_batteryPercent = 0;

static uint16_t readAdcMilliVoltsAverage()
{
    // 丢弃第一笔读数，让 ADC 多一次采样机会适应刚打开的分压节点。
    (void)analogReadMilliVolts(BATTERY_DETECT_PIN);
    delay(ADC_SAMPLE_DELAY_MS);

    uint32_t sumMv = 0;
    for (uint8_t i = 0; i < ADC_SAMPLE_COUNT; i++) {
        sumMv += analogReadMilliVolts(BATTERY_DETECT_PIN);
        delay(ADC_SAMPLE_DELAY_MS);
    }

    return (uint16_t)(sumMv / ADC_SAMPLE_COUNT);
}

static uint8_t percentFromVoltageMv(uint16_t voltageMv)
{
    const uint8_t last = (uint8_t)(sizeof(BATTERY_CURVE) / sizeof(BATTERY_CURVE[0]) - 1);

    if (voltageMv >= BATTERY_CURVE[0].voltageMv) {
        return 100;
    }

    if (voltageMv <= BATTERY_CURVE[last].voltageMv) {
        return 0;
    }

    for (uint8_t i = 0; i < last; i++) {
        const BatteryCurvePoint hi = BATTERY_CURVE[i];
        const BatteryCurvePoint lo = BATTERY_CURVE[i + 1];

        if (voltageMv <= hi.voltageMv && voltageMv >= lo.voltageMv) {
            const uint16_t voltageSpan = hi.voltageMv - lo.voltageMv;
            const uint16_t voltageOffset = voltageMv - lo.voltageMv;
            const uint8_t percentSpan = hi.percent - lo.percent;

            // 整数插值并四舍五入，输出仍然是 0~100 的连续整数。
            return lo.percent +
                   (uint8_t)(((uint32_t)voltageOffset * percentSpan + voltageSpan / 2) /
                             voltageSpan);
        }
    }

    return 0;
}

static uint16_t adcMvToBatteryMv(uint16_t adcMv)
{
    const float batteryMv = (float)adcMv *
                            DIVIDER_RESTORE_RATIO *
                            VOLTAGE_CALIBRATION;

    if (batteryMv <= 0.0f) {
        return 0;
    }

    if (batteryMv >= 65535.0f) {
        return 65535;
    }

    return (uint16_t)(batteryMv + 0.5f);
}

static void measureNow()
{
    digitalWrite(DETECT_CONTROL_PIN, DETECT_ENABLE_LEVEL);
    delay(DETECT_SETTLE_MS);

    const uint16_t adcMv = readAdcMilliVoltsAverage();

    digitalWrite(DETECT_CONTROL_PIN, DETECT_DISABLE_LEVEL);

    const uint16_t measuredBatteryMv = adcMvToBatteryMv(adcMv);

    if (!s_hasReading) {
        s_batteryVoltageMv = measuredBatteryMv;
    } else {
        s_batteryVoltageMv =
            (uint16_t)(((uint32_t)s_batteryVoltageMv * FILTER_OLD_WEIGHT +
                        (uint32_t)measuredBatteryMv * FILTER_NEW_WEIGHT) /
                       (FILTER_OLD_WEIGHT + FILTER_NEW_WEIGHT));
    }

    s_batteryPercent = percentFromVoltageMv(s_batteryVoltageMv);
    s_hasReading = true;
    s_lastMeasureMs = millis();
}

void battery_monitor_begin()
{
    // 先把输出锁存值设为“释放”，再切到开漏输出，减少初始化瞬间误导通。
    digitalWrite(DETECT_CONTROL_PIN, DETECT_DISABLE_LEVEL);
    pinMode(DETECT_CONTROL_PIN, OUTPUT_OPEN_DRAIN);
    digitalWrite(DETECT_CONTROL_PIN, DETECT_DISABLE_LEVEL);

    pinMode(BATTERY_DETECT_PIN, INPUT);

    // 11dB 衰减让 ESP32 ADC 能读取约 3V 以上的分压电压，适合本电路。
    analogSetPinAttenuation(BATTERY_DETECT_PIN, ADC_11db);

    s_hasReading = false;
    s_lastMeasureMs = 0;
    s_batteryVoltageMv = 0;
    s_batteryPercent = 0;
}

void battery_monitor_update()
{
    const uint32_t now = millis();

    if (s_hasReading && (now - s_lastMeasureMs) < MEASURE_INTERVAL_MS) {
        return;
    }

    measureNow();
}

uint8_t battery_monitor_percent()
{
    return s_batteryPercent;
}

float battery_monitor_voltage()
{
    return (float)s_batteryVoltageMv / 1000.0f;
}

bool battery_monitor_hasReading()
{
    return s_hasReading;
}
