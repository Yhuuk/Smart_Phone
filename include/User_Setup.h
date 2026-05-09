#pragma once

// 1) 选择驱动芯片
#define ILI9341_DRIVER  // ILI9341 通用驱动 :contentReference[oaicite:4]{index=4}

// 2) 选择 SPI 端口：你要用 HSPI，就打开这个宏
// #define USE_VSPI_PORT   // ESP32 默认 VSPI，取消注释可用 HSPI :contentReference[oaicite:5]{index=5}

// 3) 定义 SPI 引脚（按你实际接线修改）
#define TFT_MISO 19
#define TFT_MOSI 23
#define TFT_SCLK 18
#define TFT_CS   5
#define TFT_DC   22      //2和16，22

// RST：如果你的屏幕RST脚没接（绑到3.3V或ESP32复位），就用 -1
#define TFT_RST  32   //32
// #define TFT_RST -1

// 4) 背光控制
#define TFT_BL 21    //7和21
#define TFT_BACKLIGHT_ON HIGH   // 背光“亮”的电平，HIGH/LOW 二选一 :contentReference[oaicite:6]{index=6}

// 5) SPI 频率（ILI9341 一般 40MHz OK，过高可能花屏）:contentReference[oaicite:7]{index=7}
#define SPI_FREQUENCY      10000000
#define SPI_READ_FREQUENCY 20000000

// 6) 字库（先开常用的，避免你示例用到字体却没编进来）
#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_FONT6
#define LOAD_FONT7
#define LOAD_FONT8
#define LOAD_GFXFF
#define SMOOTH_FONT