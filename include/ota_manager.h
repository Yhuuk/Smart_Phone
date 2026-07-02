#pragma once

#include <Arduino.h>

class OtaManager {
public:
    // deviceId/version/manifestUrl 都会复制保存，后续 NVS 配置变化不会影响当前连接。
    void begin(const char *deviceId, const char *version, const char *manifestUrl);

    // MQTT 控制指令只需要调用这个函数；真正的 HTTP 下载会在 update() 中执行。
    void requestCheck();

    // 放在 loop() 中调用。没有 pending 请求时几乎不做事，不影响原有聊天功能。
    void update(bool wifiConnected);

    bool busy() const { return _busy; }

private:
    bool fetchManifestAndUpdate();
    bool runHttpUpdate(const String& binUrl, const String& md5, uint32_t expectedSize);

    String _deviceId;
    String _version;
    String _manifestUrl;
    bool _pending = false;
    bool _busy = false;
};
