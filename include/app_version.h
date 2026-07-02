#pragma once

// 每次发布 OTA 固件时递增这个版本号。
// 服务器 manifest 返回的 version 和这里不同，设备才会尝试下载新固件。
static const char *APP_VERSION = "0.1.0";
