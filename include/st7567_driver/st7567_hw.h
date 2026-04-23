/**
 * @file st7567_hw.h
 * @author Kulib
 * @brief 定义 ST7567 (12832) 屏幕的硬件接口
 * @version 0.1
 * @date 2026-04-24
 * */
#ifndef ST7567_HW_H
#define ST7567_HW_H

#include <stdint.h>
#include "gpio_define.h"

void LCD_Init(); // 初始化屏幕

void LCD_Clear(); // 清屏

/*
@brief 在指定坐标点亮一个像素
@param x 水平坐标，范围 [0, 127]
@param y 垂直坐标，范围 [0, 31]
@note 该函数会更新内部帧缓冲并刷新对应页的数据到屏幕
*/
void LCD_DrawPixel(int x, int y);

// x,y: 左上角坐标，y 必须是 8 的倍数（0/8/16/24）
// 支持字符：' '、'.'、'0'-'9'
// 字符宽 5px＋1px 间距，共 6px
void LCD_DrawChar(int x, int y, char c);
void LCD_DrawStr(int x, int y, const char *s);

#endif // ST7567_HW_H