#include <Arduino.h>
#include <lvgl.h>
#include <TFT_eSPI.h>

/**************************************************
 * 这个显示的逻辑是：LVGL先在缓冲区画好颜色，再把整个颜色
 *      缓冲区交给pushPixels()。
 * 遇到的问题：颜色显示不正确
 * ->如果缓冲区的16位像素排列方式和TFT_eSPI预期的不一致就会导致显示颜色异常
 * 
 *************************************************/

static  TFT_eSPI tft;


static  constexpr   uint16_t BUF_LINES  =  20;  //20行
static  lv_color_t buf1[320 * BUF_LINES];   //lv_color_t是缓冲区
static  lv_color_t buf2[320 * BUF_LINES];
static  lv_disp_draw_buf_t  draw_buf;

static void lvgl_flush_cb(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p)
{
    const int32_t w = area->x2 - area->x1 +1;
    const int32_t h = area->y2 - area->y1 +1;

    tft.startWrite();
    tft.setAddrWindow(area->x1, area->y1, w, h);


    tft.pushPixels((uint16_t *)color_p, (uint32_t)(w * h));

    tft.endWrite();

    lv_disp_flush_ready(disp);
}


/*在main.cpp中调用的初始化函数 */
void lvgl_port_init()
{
    lv_init();

    tft.init();
    tft.setRotation(2);         //屏幕旋转
    tft.setSwapBytes(true);     //颜色不对再改，（不知道这个函数的具体作用）
                                //把缓冲区输出到屏幕的每个像素的两个字节交换一下顺序
    const int32_t   scr_w = tft.width();
    const int32_t   scr_h = tft.height();

    lv_disp_draw_buf_init(&draw_buf,buf1,buf2,scr_w * BUF_LINES); 
    
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = scr_w;
    disp_drv.ver_res = scr_h;
    disp_drv.flush_cb = lvgl_flush_cb;
    disp_drv.draw_buf = &draw_buf;
    lv_disp_drv_register(&disp_drv);
}

