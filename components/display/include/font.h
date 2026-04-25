/**
 * @file font.h
 * @author Kulib
 * @brief 声明字体相关的接口
 * @version 0.1
 * @date 2026-04-24
 * */

#ifndef FONT_H
#define FONT_H

#include <stdint.h>

typedef struct {
    const uint8_t *data;         // 字体数据（每个字符 bytes_per_char 字节）
    uint8_t width;               // 字符宽度
    uint8_t height;              // 字符高度
    char start_char;             // 起始字符（通常是空格 ' '）
    uint8_t bytes_per_char;      // 每字符占用字节数
} font_t;

extern const font_t ascii_16X32;

extern const font_t ascii_8x16;

extern const font_t ascii_5x8;

#endif // FONT_H
