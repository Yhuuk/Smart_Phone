#include "app_config.h"

#include "WIFI_MQTT.h"

#include <Preferences.h>

static const char *CFG_NVS_NAMESPACE = "appcfg";

// Preferences 的 key 最长 15 个字符，这里统一使用短 key。
static const char *KEY_WIFI_SSID = "wifi_ssid";
static const char *KEY_WIFI_PASS = "wifi_pass";
static const char *KEY_MQTT_HOST = "mqtt_host";
static const char *KEY_MQTT_PORT = "mqtt_port";
static const char *KEY_TOPIC_TX = "topic_tx";
static const char *KEY_TOPIC_RX = "topic_rx";
static const char *KEY_CMD_TOPIC = "cmd_topic";
static const char *KEY_HTTP_BASE = "http_base";
static const char *KEY_OTA_URL = "ota_url";
static const char *KEY_DEVICE_ID = "device_id";
static const char *KEY_ROOM_ID = "room_id";
static const char *KEY_CTRL_TOKEN = "ctrl_token";

struct RuntimeConfigSource {
    bool wifiSsid = false;
    bool wifiPass = false;
    bool mqttHost = false;
    bool mqttPort = false;
    bool topicTx = false;
    bool topicRx = false;
    bool cmdTopic = false;
    bool httpServerBase = false;
    bool otaManifestUrl = false;
    bool deviceId = false;
    bool roomId = false;
    bool controlToken = false;
};

static RuntimeConfig *g_config = nullptr;
static RuntimeConfigSource *g_source = nullptr;

static void ensureConfigStorage()
{
    if (g_config == nullptr) {
        g_config = new RuntimeConfig();
    }
    if (g_source == nullptr) {
        g_source = new RuntimeConfigSource();
    }
}

static const char *sourceName(bool fromNvs)
{
    return fromNvs ? "NVS" : "DEFAULT";
}

static String maskSecret(const String& value)
{
    if (value.length() == 0) return "(empty)";
    return String("***(") + value.length() + " chars)";
}

static String readStringField(Preferences& prefs,
                              const char *key,
                              const String& defaultValue,
                              bool& fromNvs)
{
    fromNvs = prefs.isKey(key);
    if (!fromNvs) return defaultValue;
    return prefs.getString(key, defaultValue);
}

static uint16_t readPortField(Preferences& prefs,
                              const char *key,
                              uint16_t defaultValue,
                              bool& fromNvs)
{
    fromNvs = prefs.isKey(key);
    if (!fromNvs) return defaultValue;

    uint32_t value = prefs.getUInt(key, defaultValue);
    if (value == 0 || value > 65535) {
        Serial.print("[CFG] invalid mqtt_port in NVS, fallback to default: ");
        Serial.println(defaultValue);
        fromNvs = false;
        return defaultValue;
    }

    return static_cast<uint16_t>(value);
}

static void loadConfig()
{
    ensureConfigStorage();

    RuntimeConfig cfg;
    RuntimeConfigSource src;

    Preferences prefs;
    bool opened = prefs.begin(CFG_NVS_NAMESPACE, true);

    if (!opened) {
        Serial.println("[CFG] NVS open failed, using code defaults");
    }

    cfg.wifiSsid = opened ? readStringField(prefs, KEY_WIFI_SSID, WIFI_SSID, src.wifiSsid)
                          : String(WIFI_SSID);
    cfg.wifiPass = opened ? readStringField(prefs, KEY_WIFI_PASS, WIFI_PASS, src.wifiPass)
                          : String(WIFI_PASS);
    cfg.mqttHost = opened ? readStringField(prefs, KEY_MQTT_HOST, MQTT_HOST, src.mqttHost)
                          : String(MQTT_HOST);
    cfg.mqttPort = opened ? readPortField(prefs, KEY_MQTT_PORT, MQTT_PORT, src.mqttPort)
                          : MQTT_PORT;
    cfg.topicTx = opened ? readStringField(prefs, KEY_TOPIC_TX, TOPIC_TX, src.topicTx)
                         : String(TOPIC_TX);
    cfg.topicRx = opened ? readStringField(prefs, KEY_TOPIC_RX, TOPIC_RX, src.topicRx)
                         : String(TOPIC_RX);
    cfg.httpServerBase = opened ? readStringField(prefs, KEY_HTTP_BASE, HTTP_SERVER_BASE, src.httpServerBase)
                                : String(HTTP_SERVER_BASE);
    cfg.deviceId = opened ? readStringField(prefs, KEY_DEVICE_ID, DEVICE_ID, src.deviceId)
                          : String(DEVICE_ID);
    cfg.roomId = opened ? readStringField(prefs, KEY_ROOM_ID, ROOM_ID, src.roomId)
                        : String(ROOM_ID);

    String defaultCmdTopic = String("phone/") + cfg.deviceId + "/cmd";
    cfg.cmdTopic = opened ? readStringField(prefs, KEY_CMD_TOPIC, defaultCmdTopic, src.cmdTopic)
                          : defaultCmdTopic;

    String defaultOtaUrl = cfg.httpServerBase + "/api/ota/manifest";
    cfg.otaManifestUrl = opened ? readStringField(prefs, KEY_OTA_URL, defaultOtaUrl, src.otaManifestUrl)
                                : defaultOtaUrl;

    // 控制 token 用来避免任何人随便发 MQTT 指令。正式使用前建议通过 NVS 改成自己的长随机串。
    cfg.controlToken = opened ? readStringField(prefs, KEY_CTRL_TOKEN, "dev-token-change-me", src.controlToken)
                              : String("dev-token-change-me");

    if (opened) {
        prefs.end();
    }

    *g_config = cfg;
    *g_source = src;
}

static bool putStringField(Preferences& prefs,
                           JsonObjectConst cfg,
                           const char *jsonKey,
                           const char *nvsKey,
                           bool allowEmpty = false)
{
    JsonVariantConst value = cfg[jsonKey];
    if (value.isNull()) return false;

    const char *raw = value.as<const char*>();
    if (raw == nullptr) {
        Serial.print("[CFG] skip non-string field: ");
        Serial.println(jsonKey);
        return false;
    }

    String text = raw;
    text.trim();
    if (!allowEmpty && text.length() == 0) {
        Serial.print("[CFG] skip empty field: ");
        Serial.println(jsonKey);
        return false;
    }

    prefs.putString(nvsKey, text);
    Serial.print("[CFG] NVS updated: ");
    Serial.println(jsonKey);
    return true;
}

static bool putStringFieldAlias(Preferences& prefs,
                                JsonObjectConst cfg,
                                const char *jsonKey,
                                const char *aliasKey,
                                const char *nvsKey,
                                bool allowEmpty = false)
{
    bool changed = putStringField(prefs, cfg, jsonKey, nvsKey, allowEmpty);
    if (!changed) {
        changed = putStringField(prefs, cfg, aliasKey, nvsKey, allowEmpty);
    }
    return changed;
}

static bool putPortField(Preferences& prefs, JsonObjectConst cfg)
{
    JsonVariantConst value = cfg["mqtt_port"];
    if (value.isNull()) return false;

    uint32_t port = value.as<uint32_t>();
    if (port == 0 || port > 65535) {
        Serial.print("[CFG] skip invalid mqtt_port: ");
        Serial.println(port);
        return false;
    }

    prefs.putUInt(KEY_MQTT_PORT, port);
    Serial.print("[CFG] NVS updated: mqtt_port=");
    Serial.println(port);
    return true;
}

void app_config_begin()
{
    loadConfig();
    app_config_print();
}

const RuntimeConfig& app_config()
{
    ensureConfigStorage();
    return *g_config;
}

void app_config_print()
{
    ensureConfigStorage();

    Serial.println("[CFG] ===== Runtime config =====");
    Serial.print("[CFG] wifi_ssid = ");
    Serial.print(g_config->wifiSsid);
    Serial.print(" (");
    Serial.print(sourceName(g_source->wifiSsid));
    Serial.println(")");

    Serial.print("[CFG] wifi_pass = ");
    Serial.print(maskSecret(g_config->wifiPass));
    Serial.print(" (");
    Serial.print(sourceName(g_source->wifiPass));
    Serial.println(")");

    Serial.print("[CFG] mqtt_host = ");
    Serial.print(g_config->mqttHost);
    Serial.print(" (");
    Serial.print(sourceName(g_source->mqttHost));
    Serial.println(")");

    Serial.print("[CFG] mqtt_port = ");
    Serial.print(g_config->mqttPort);
    Serial.print(" (");
    Serial.print(sourceName(g_source->mqttPort));
    Serial.println(")");

    Serial.print("[CFG] topic_tx = ");
    Serial.print(g_config->topicTx);
    Serial.print(" (");
    Serial.print(sourceName(g_source->topicTx));
    Serial.println(")");

    Serial.print("[CFG] topic_rx = ");
    Serial.print(g_config->topicRx);
    Serial.print(" (");
    Serial.print(sourceName(g_source->topicRx));
    Serial.println(")");

    Serial.print("[CFG] cmd_topic = ");
    Serial.print(g_config->cmdTopic);
    Serial.print(" (");
    Serial.print(sourceName(g_source->cmdTopic));
    Serial.println(")");

    Serial.print("[CFG] http_server_base = ");
    Serial.print(g_config->httpServerBase);
    Serial.print(" (");
    Serial.print(sourceName(g_source->httpServerBase));
    Serial.println(")");

    Serial.print("[CFG] ota_manifest_url = ");
    Serial.print(g_config->otaManifestUrl);
    Serial.print(" (");
    Serial.print(sourceName(g_source->otaManifestUrl));
    Serial.println(")");

    Serial.print("[CFG] device_id = ");
    Serial.print(g_config->deviceId);
    Serial.print(" (");
    Serial.print(sourceName(g_source->deviceId));
    Serial.println(")");

    Serial.print("[CFG] room_id = ");
    Serial.print(g_config->roomId);
    Serial.print(" (");
    Serial.print(sourceName(g_source->roomId));
    Serial.println(")");

    Serial.print("[CFG] control_token = ");
    Serial.print(maskSecret(g_config->controlToken));
    Serial.print(" (");
    Serial.print(sourceName(g_source->controlToken));
    Serial.println(")");
}

bool app_config_update_from_json(JsonObjectConst cfg)
{
    if (cfg.isNull()) {
        Serial.println("[CFG] config_update missing config object");
        return false;
    }

    Preferences prefs;
    if (!prefs.begin(CFG_NVS_NAMESPACE, false)) {
        Serial.println("[CFG] NVS open failed, config not saved");
        return false;
    }

    bool changed = false;
    changed |= putStringField(prefs, cfg, "wifi_ssid", KEY_WIFI_SSID);
    changed |= putStringField(prefs, cfg, "wifi_pass", KEY_WIFI_PASS, true);
    changed |= putStringField(prefs, cfg, "mqtt_host", KEY_MQTT_HOST);
    changed |= putPortField(prefs, cfg);
    changed |= putStringField(prefs, cfg, "topic_tx", KEY_TOPIC_TX);
    changed |= putStringField(prefs, cfg, "topic_rx", KEY_TOPIC_RX);
    changed |= putStringField(prefs, cfg, "cmd_topic", KEY_CMD_TOPIC);
    changed |= putStringFieldAlias(prefs, cfg, "http_server_base", "http_base", KEY_HTTP_BASE);
    changed |= putStringFieldAlias(prefs, cfg, "ota_manifest_url", "ota_url", KEY_OTA_URL);
    changed |= putStringField(prefs, cfg, "device_id", KEY_DEVICE_ID);
    changed |= putStringField(prefs, cfg, "room_id", KEY_ROOM_ID);
    changed |= putStringFieldAlias(prefs, cfg, "control_token", "ctrl_token", KEY_CTRL_TOKEN);

    prefs.end();

    if (changed) {
        loadConfig();
        app_config_print();
    } else {
        Serial.println("[CFG] no valid config field found");
    }

    return changed;
}
