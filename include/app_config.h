#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

struct RuntimeConfig {
    String wifiSsid;
    String wifiPass;
    String mqttHost;
    uint16_t mqttPort = 1883;
    String topicTx;
    String topicRx;
    String cmdTopic;
    String httpServerBase;
    String otaManifestUrl;
    String deviceId;
    String roomId;
    String controlToken;
};

// 启动时调用一次：先读 NVS，没有写入过的字段会使用代码里的默认配置。
void app_config_begin();

// 当前生效配置。注意：WiFi/MQTT 已经连接后，修改 NVS 不会自动重连，通常重启后生效。
const RuntimeConfig& app_config();

// 统一把所有配置打印到串口，并标注每一项来自 NVS 还是代码默认值。
void app_config_print();

// 从 MQTT 控制指令里的 "config" 对象更新 NVS。
// 只会写入 JSON 中出现的字段，未出现的字段保持原样。
bool app_config_update_from_json(JsonObjectConst cfg);
