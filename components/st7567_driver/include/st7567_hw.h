/**
 * @file st7567_hw.h
 * @author Kulib
 * @brief 声明 ST7567 屏幕的硬件接口
 * @date 2026-04-26
 * */

#ifndef ST7567_HW_H
#define ST7567_HW_H

#include <stddef.h>
#include <stdint.h>
#include "gpio_define.h"

// 12832 屏幕，使用 Page 0-3
#ifndef LCD_WIDTH
#define LCD_WIDTH   128
#endif

#ifndef LCD_HEIGHT
#define LCD_HEIGHT  32
#endif

#ifndef LCD_PAGES
#define LCD_PAGES   (LCD_HEIGHT / 8)
#endif

// 列反向
#ifndef LCD_SEG_REMAP
#define LCD_SEG_REMAP        0   // 0:A0 普通, 1:A1 反向
#endif

// 行反向
#ifndef LCD_COM_REVERSE
#define LCD_COM_REVERSE      1   // 0:C0 普通, 1:C8 反向
#endif

// 偏置比选择
#ifndef LCD_BIAS_1_7
#define LCD_BIAS_1_7         0   // 0:A2(1/9), 1:A3(1/7)
#endif

// 电阻调节比率 (对比度相关)
#ifndef LCD_REG_RATIO
#define LCD_REG_RATIO        0x00  // RR=3.0
#endif

// 驱动电压等级 (对比度相关)
#ifndef LCD_EV_LEVEL
#define LCD_EV_LEVEL         0x2C  // 对比度 EV=0x2C
#endif

// 列起始偏移
#ifndef LCD_COL_OFFSET
#define LCD_COL_OFFSET       0   // 132 列控制器的 128 列显示区域通常需要偏移 4 列
#endif

// 反显控制
#ifndef LCD_INVERSE
#define LCD_INVERSE          0   // 0:正常显示, 1:反显
#endif

/**
 * @brief 初始化 LCD 屏幕，配置 GPIO 引脚并执行复位和基本设置
 * @note 该函数会直接操作 GPIO 引脚进行屏幕初始化，完成后会清屏。请确保在调用其他 LCD 函数前先调用该函数。
 */
void LCD_Init(); // 初始化屏幕

/**
 * @brief 清屏函数，将屏幕内容全部设置为 0（黑色）
 * @note 该函数会直接写入 LCD 数据寄存器，清空整个显示区域，需要自行同步帧缓冲区状态（如果有的话）
 */
void LCD_Clear(); // 清屏

/**
 * @brief 设置对比度
 * @param contrast 对比度,取值为 0 - 63
 */
void LCD_set_contrast(uint8_t contrast);

/**
 * @brief 设置行偏移
 * @param offset 行偏移,取值为 0 - 63 
 */
void LCD_set_col_offset(uint8_t offset);

/**
 * @brief 将帧缓冲区内容刷新到屏幕
 * @param s_framebuffer 2D 数组，尺寸为 [LCD_PAGES][LCD_WIDTH]，每个元素为一个字节的像素数据
 * @param s_col_offset 列偏移量，适配不同屏幕的列起始位置（通常为 0 或 4）
 * @note 该函数会根据 s_col_offset 自动调整列地址，适配 132 列控制器的 128 列显示区域
 */
void LCD_flush_buffer(uint8_t s_framebuffer[LCD_PAGES][LCD_WIDTH], uint8_t s_col_offset);

#endif // ST7567_HW_H