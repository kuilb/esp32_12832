/**
 * @file display_gfx.h
 * @author Kulib
 * @brief 定义显示图形相关的接口
 * @version 0.1
 * @date 2026-04-24
 * */

#ifndef DISPLAY_GFX_H
#define DISPLAY_GFX_H

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include "display_gfx/font.h"
#include "display_gfx/display_gfx.h"
#include "st7567_driver/st7567_hw.h"

typedef enum {
    GFX_COLOR_BLACK = 0, // 灭（通常对应数据 0）
    GFX_COLOR_WHITE = 1, // 亮（通常对应数据 1）
    GFX_COLOR_INVERT = 2  // 反转（异或操作，实现遮盖处取反）
} gfx_color_t;

/**
 * @brief 清空显示缓冲区
 * @param color 填充颜色
 */
void gfx_clear_buffer(gfx_color_t color);

/**
 * @brief 刷新显示
 */
void gfx_display();

/**
 * @brief 设置反转显示
 * @param invert 是否反转
 */
void gfx_set_invert(bool invert);

/**
 * @brief 绘制像素
 * @param x X 坐标
 * @param y Y 坐标
 * @param color 颜色
 */
void draw_pixel(uint16_t x, uint16_t y, gfx_color_t color);

/**
 * @brief 绘制直线
 * @param x0 起点 X 坐标
 * @param y0 起点 Y 坐标
 * @param x1 终点 X 坐标
 * @param y1 终点 Y 坐标
 * @param color 颜色
 */
void draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, gfx_color_t color);

/**
 * @brief 绘制矩形
 * @param x 左上角 X 坐标
 * @param y 左上角 Y 坐标
 * @param w 宽度
 * @param h 高度
 * @param color 颜色
 */
void draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, gfx_color_t color);

/**
 * @brief 填充矩形
 * @param x 左上角 X 坐标
 * @param y 左上角 Y 坐标
 * @param w 宽度
 * @param h 高度
 * @param color 颜色
 */
void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, gfx_color_t color);

/**
 * @brief 绘制圆形
 * @param x0 圆心 X 坐标
 * @param y0 圆心 Y 坐标
 * @param r 半径
 * @param color 颜色
 */
void draw_circle(uint16_t x0, uint16_t y0, uint16_t r, gfx_color_t color);

/**
 * @brief 填充圆形
 * @param x0 圆心 X 坐标
 * @param y0 圆心 Y 坐标
 * @param r 半径
 * @param color 颜色
 */
void fill_circle(uint16_t x0, uint16_t y0, uint16_t r, gfx_color_t color);

/**
 * @brief 绘制字符
 * @param x X 坐标
 * @param y Y 坐标
 * @param c 字符
 * @param font 字体
 * @param color 颜色
 */
void draw_ascii_char(uint16_t x, uint16_t y, char c, font_t font, gfx_color_t color);

/**
 * @brief 绘制字符串
 * @param x X 坐标
 * @param y Y 坐标
 * @param str 字符串
 * @param font 字体
 * @param color 颜色
 */
void draw_ascii_string(uint16_t x, uint16_t y, const char *str, font_t font, gfx_color_t color);

#endif // DISPLAY_GFX_H