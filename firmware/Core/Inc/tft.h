#ifndef __TFT_H
#define __TFT_H

#include "main.h"
#include "lvgl/lvgl.h"

#define CS_LOW() HAL_GPIO_WritePin(TFT_CS_GPIO_Port,TFT_CS_Pin,GPIO_PIN_RESET)
#define CS_HIGH() HAL_GPIO_WritePin(TFT_CS_GPIO_Port,TFT_CS_Pin,GPIO_PIN_SET)

#define DC_LOW() HAL_GPIO_WritePin(TFT_DC_GPIO_Port,TFT_DC_Pin,GPIO_PIN_RESET)
#define DC_HIGH() HAL_GPIO_WritePin(TFT_DC_GPIO_Port,TFT_DC_Pin,GPIO_PIN_SET)

void ST7789_Init(void);
void ST7789_Fill_Color(uint16_t color);
void ST7789_Fill_Color_DMA(uint16_t color);
void my_disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);
void ST7789_SetAddr(uint16_t x1,uint16_t y1,uint16_t x2,uint16_t y2);
#endif
