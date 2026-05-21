#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>   //干嘛的？

#define LV_COLOR_DEPTH      16   //rgb565
#define LV_MEM_SIZE         (48U * 1024U)
#define LV_USE_OS           0    //这是什么，不懂意思


#define LV_HOR_RES_MAX      320  //水平分辨率
#define LV_VER_RES_MAX      240 //垂直分辨率ifn


#define LV_USE_LOG          0 //关闭

#define LV_FONT_MONTSERRAT_14   1
#define LV_FONT_MONTSERRAT_16   1
#define LV_FONT_MONTSERRAT_24   1
#define LV_FONT_MONTSERRAT_48   1
#define LV_FONT_DEFAULT         &lv_font_montserrat_24  //lv_font_montserrat_14必须是小写

#define LV_USE_LABEL            1
#define LV_USE_BTN              1   //这是什么，不是很懂

#endif