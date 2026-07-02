#include "ota_manager.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Update.h>

static const uint32_t OTA_HTTP_TIMEOUT_MS = 8000;

void OtaManager::begin(const char *deviceId, const char *version, const char *manifestUrl)
{
    _deviceId = deviceId ? deviceId : "";
    _version = version ? version : "";
    _manifestUrl = manifestUrl ? manifestUrl : "";

    Serial.print("[OTA] device=");
    Serial.print(_deviceId);
    Serial.print(" version=");
    Serial.print(_version);
    Serial.print(" manifest=");
    Serial.println(_manifestUrl);
}

void OtaManager::requestCheck()
{
    _pending = true;
    Serial.println("[OTA] check requested");
}

void OtaManager::update(bool wifiConnected)
{
    if (!_pending || _busy) return;

    if (!wifiConnected) {
        Serial.println("[OTA] pending, wait for WiFi");
        return;
    }

    _pending = false;
    _busy = true;
    fetchManifestAndUpdate();
    _busy = false;
}

bool OtaManager::fetchManifestAndUpdate()
{
    if (_manifestUrl.length() == 0 || _deviceId.length() == 0) {
        Serial.println("[OTA] manifest url or device id is empty");
        return false;
    }

    String url = _manifestUrl;
    url += "?device=";
    url += _deviceId;
    url += "&version=";
    url += _version;

    Serial.print("[OTA] GET ");
    Serial.println(url);

    HTTPClient http;
    http.setTimeout(OTA_HTTP_TIMEOUT_MS);

    if (!http.begin(url)) {
        Serial.println("[OTA] manifest begin failed");
        return false;
    }

    int code = http.GET();
    Serial.print("[OTA] manifest code=");
    Serial.println(code);

    if (code != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    String payload = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, payload);
    if (err) {
        Serial.print("[OTA] manifest parse error: ");
        Serial.println(err.c_str());
        return false;
    }

    const char *target = doc["target"] | "";
    if (target[0] != '\0' && strcmp(target, _deviceId.c_str()) != 0 && strcmp(target, "all") != 0) {
        Serial.print("[OTA] ignore manifest target=");
        Serial.println(target);
        return false;
    }

    String newVersion = doc["version"] | "";
    String binUrl = doc["url"] | "";
    String md5 = doc["md5"] | "";
    bool force = doc["force"] | false;
    uint32_t expectedSize = doc["size"] | 0;

    if (binUrl.length() == 0) {
        Serial.println("[OTA] no firmware url, already up to date or server disabled");
        return false;
    }

    if (!force && newVersion.length() > 0 && newVersion == _version) {
        Serial.print("[OTA] already at version ");
        Serial.println(_version);
        return false;
    }

    Serial.print("[OTA] update to version=");
    Serial.print(newVersion.length() ? newVersion : "(unknown)");
    Serial.print(" size=");
    Serial.print(expectedSize);
    Serial.print(" md5=");
    Serial.println(md5.length() ? md5 : "(none)");

    return runHttpUpdate(binUrl, md5, expectedSize);
}

bool OtaManager::runHttpUpdate(const String& binUrl, const String& md5, uint32_t expectedSize)
{
    Serial.print("[OTA] firmware GET ");
    Serial.println(binUrl);

    HTTPClient http;
    http.setTimeout(OTA_HTTP_TIMEOUT_MS);

    if (!http.begin(binUrl)) {
        Serial.println("[OTA] firmware begin failed");
        return false;
    }

    int code = http.GET();
    Serial.print("[OTA] firmware code=");
    Serial.println(code);

    if (code != HTTP_CODE_OK) {
        http.end();
        return false;
    }

    int contentLength = http.getSize();
    if (expectedSize > 0 && contentLength > 0 && static_cast<uint32_t>(contentLength) != expectedSize) {
        Serial.print("[OTA] size warning, manifest=");
        Serial.print(expectedSize);
        Serial.print(" http=");
        Serial.println(contentLength);
    }

    size_t updateSize = contentLength > 0 ? static_cast<size_t>(contentLength) : UPDATE_SIZE_UNKNOWN;
    if (!Update.begin(updateSize)) {
        Serial.print("[OTA] Update.begin failed: ");
        Update.printError(Serial);
        http.end();
        return false;
    }

    if (md5.length() > 0 && !Update.setMD5(md5.c_str())) {
        Serial.println("[OTA] invalid md5 string");
        Update.abort();
        http.end();
        return false;
    }

    WiFiClient *stream = http.getStreamPtr();
    size_t written = Update.writeStream(*stream);

    Serial.print("[OTA] written=");
    Serial.println(written);

    if (contentLength > 0 && written != static_cast<size_t>(contentLength)) {
        Serial.println("[OTA] written size mismatch");
        Update.abort();
        http.end();
        return false;
    }

    if (!Update.end()) {
        Serial.print("[OTA] Update.end failed: ");
        Update.printError(Serial);
        http.end();
        return false;
    }

    if (!Update.isFinished()) {
        Serial.println("[OTA] update not finished");
        http.end();
        return false;
    }

    http.end();

    Serial.println("[OTA] success, rebooting...");
    delay(300);
    ESP.restart();
    return true;
}
