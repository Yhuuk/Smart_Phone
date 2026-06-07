#include "phone_time.h"

#include <stdio.h>
#include <time.h>

// ESP32 Arduino 的系统时间需要先通过 SNTP/NTP 校准。
// 中国大陆固定使用 UTC+8，没有夏令时。POSIX TZ 字符串里的符号方向和
// 常见写法相反，所以 UTC+8 要写成 "CST-8"。
static const char *TZ_CHINA = "CST-8";

// NTP 服务器按“国内优先、公共备用”的顺序配置。configTzTime() 会把这些
// 服务器交给 ESP32 的 SNTP 后台任务，不需要我们在 loop() 里手动发包。
static const char *NTP_SERVER_1 = "ntp.aliyun.com";
static const char *NTP_SERVER_2 = "pool.ntp.org";
static const char *NTP_SERVER_3 = "time.windows.com";

// 用一个保守的时间下限判断“系统时间是否可信”。
// ESP32 刚开机且未同步时，time(nullptr) 往往是 1970 年附近；只要大于
// 2024-01-01，就基本可以确认已经被 NTP 或其它时间源校准过。
static constexpr time_t VALID_TIME_EPOCH = 1704067200; // 2024-01-01 00:00:00 UTC

// 如果 WiFi 已连接但暂时没有同步成功，每 30 秒重新触发一次 NTP 配置。
// 这个间隔避免在网络较差时反复配置 SNTP，同时又能在路由器恢复后自动补救。
static constexpr uint32_t NTP_RETRY_MS = 30000;

static bool g_ntpStarted = false;
static bool g_timeReady = false;
static bool g_lastWifiConnected = false;
static uint32_t g_lastNtpStartMs = 0;

static bool isEpochValid(time_t now)
{
    return now >= VALID_TIME_EPOCH;
}

static void startNtp()
{
    // configTzTime() 会同时设置时区和启动 SNTP。它不是“立刻拿到时间”的
    // 同步调用，所以这里记录启动状态，后续用 time(nullptr) 检查是否成功。
    configTzTime(TZ_CHINA, NTP_SERVER_1, NTP_SERVER_2, NTP_SERVER_3);
    g_ntpStarted = true;
    g_lastNtpStartMs = millis();
}

void phone_time_begin()
{
    // 先设置本地时区。即使 NTP 还没成功，后续 localtime_r() 也会按中国
    // 时区解释系统时间，避免 UI 层关心时区细节。
    setenv("TZ", TZ_CHINA, 1);
    tzset();

    g_ntpStarted = false;
    g_timeReady = isEpochValid(time(nullptr));
    g_lastWifiConnected = false;
    g_lastNtpStartMs = 0;
}

void phone_time_update(bool wifiConnected)
{
    const uint32_t nowMs = millis();

    if (!wifiConnected) {
        // 断网时不启动网络同步；如果之前已经同步过，系统时间会继续走。
        g_lastWifiConnected = false;
        g_timeReady = g_timeReady || isEpochValid(time(nullptr));
        return;
    }

    const bool wifiJustConnected = !g_lastWifiConnected;
    const bool retryDue = g_ntpStarted &&
                          !g_timeReady &&
                          (nowMs - g_lastNtpStartMs >= NTP_RETRY_MS);

    if (!g_ntpStarted || wifiJustConnected || retryDue) {
        startNtp();
    }

    g_lastWifiConnected = true;
    g_timeReady = g_timeReady || isEpochValid(time(nullptr));
}

bool phone_time_format(char *dateBuf,
                       size_t dateLen,
                       char *timeBuf,
                       size_t timeLen)
{
    if (!dateBuf || dateLen == 0 || !timeBuf || timeLen == 0) {
        return false;
    }

    dateBuf[0] = '\0';
    timeBuf[0] = '\0';

    const time_t now = time(nullptr);
    if (!isEpochValid(now)) {
        return false;
    }

    struct tm localTime;
    if (localtime_r(&now, &localTime) == nullptr) {
        return false;
    }

    // 不依赖系统 locale，保证星期始终显示为英文三字母缩写。
    static const char *WEEK_NAMES[] = {
        "Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"
    };

    const uint8_t weekIndex = (localTime.tm_wday >= 0 && localTime.tm_wday <= 6)
                                  ? localTime.tm_wday
                                  : 0;

    snprintf(dateBuf,
             dateLen,
             "%04d-%02d-%02d",
             localTime.tm_year + 1900,
             localTime.tm_mon + 1,
             localTime.tm_mday);

    snprintf(timeBuf,
             timeLen,
             "%s %02d:%02d",
             WEEK_NAMES[weekIndex],
             localTime.tm_hour,
             localTime.tm_min);

    g_timeReady = true;
    return true;
}

bool phone_time_ready()
{
    g_timeReady = g_timeReady || isEpochValid(time(nullptr));
    return g_timeReady;
}
