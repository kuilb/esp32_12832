/**
 * @file display_gfx.c
 * @author Kulib
 * @brief 定义显示图形相关的接口,同时管理帧缓冲区
 * @date 2026-04-24
 * */

 #include "display_gfx.h"

#ifndef LCD_WIDTH
#define LCD_WIDTH   128
#endif

#ifndef LCD_HEIGHT
#define LCD_HEIGHT  32
#endif

#ifndef LCD_PAGES
#define LCD_PAGES   (LCD_HEIGHT / 8)
#endif

// 帧缓冲区，每页 128 字节，4 页共 512 字节
static uint8_t s_framebuffer[LCD_PAGES][LCD_WIDTH];
static uint8_t s_col_offset = 0; // 偏移量

void gfx_clear_buffer(gfx_color_t color){
    uint8_t fill_byte = 0x00; // 默认黑色
    if (color == GFX_COLOR_WHITE) {
        fill_byte = 0xFF; // 白色对应全 1
    } else if (color == GFX_COLOR_INVERT) {
        // 反转颜色需要先读取当前帧缓冲内容，逐字节取反
        for (uint8_t page = 0; page < LCD_PAGES; page++) {
            for (uint16_t x = 0; x < LCD_WIDTH; x++) {
                s_framebuffer[page][x] = ~s_framebuffer[page][x];
            }
        }
        return;
    }
    // 填充整个帧缓冲
    for (uint8_t page = 0; page < LCD_PAGES; page++) {
        for (uint16_t x = 0; x < LCD_WIDTH; x++) {
            s_framebuffer[page][x] = fill_byte;
        }
    }
}

void gfx_display() {
    LCD_flush_buffer(s_framebuffer, s_col_offset);
}

void gfx_set_invert(bool invert) {
    // 反转显示需要先刷新当前帧缓冲内容，逐字节取反
    for (uint8_t page = 0; page < LCD_PAGES; page++) {
        for (uint16_t x = 0; x < LCD_WIDTH; x++) {
            s_framebuffer[page][x] = ~s_framebuffer[page][x];
        }
    }
    LCD_flush_buffer(s_framebuffer, s_col_offset);
}

void draw_pixel(uint16_t x, uint16_t y, gfx_color_t color) {
    if (x >= LCD_WIDTH || y >= LCD_HEIGHT) {
        return; // 越界检查
    }
    uint8_t page = y / 8;
    uint8_t bit_mask = 1 << (y % 8);
    switch (color) {
        case GFX_COLOR_BLACK:
            s_framebuffer[page][x] &= ~bit_mask; // 灭：对应位清 0
            break;
        case GFX_COLOR_WHITE:
            s_framebuffer[page][x] |= bit_mask;  // 亮：对应位设 1
            break;
        case GFX_COLOR_INVERT:
            s_framebuffer[page][x] ^= bit_mask;  // 反转：对应位取反
            break;
    }
}

void draw_line(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, gfx_color_t color) {
    // Bresenham 直线算法
    int16_t dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int16_t dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int16_t err = dx + dy, e2;
    while (true) {
        draw_pixel(x0, y0, color);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 >= dy) { err += dy; x0 += sx; }
        if (e2 <= dx) { err += dx; y0 += sy; }
    }
}

void draw_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, gfx_color_t color) {
    draw_line(x, y, x + w - 1, y, color);         // 上边
    draw_line(x, y + h - 1, x + w - 1, y + h - 1, color); // 下边
    draw_line(x, y, x, y + h - 1, color);         // 左边
    draw_line(x + w - 1, y, x + w - 1, y + h - 1, color); // 右边
}

void fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, gfx_color_t color) {
    for (uint16_t i = 0; i < h; i++) {
        draw_line(x, y + i, x + w - 1, y + i, color);
    }
}

void draw_circle(uint16_t x0, uint16_t y0, uint16_t r, gfx_color_t color) {
    // 中点圆算法
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    draw_pixel(x0, y0 + r, color);
    draw_pixel(x0, y0 - r, color);
    draw_pixel(x0 + r, y0, color);
    draw_pixel(x0 - r, y0, color);

    while (x < y) {
        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;

        draw_pixel(x0 + x, y0 + y, color);
        draw_pixel(x0 - x, y0 + y, color);
        draw_pixel(x0 + x, y0 - y, color);
        draw_pixel(x0 - x, y0 - y, color);
        draw_pixel(x0 + y, y0 + x, color);
        draw_pixel(x0 - y, y0 + x, color);
        draw_pixel(x0 + y, y0 - x, color);
        draw_pixel(x0 - y, y0 - x, color);
    }
}

void fill_circle(uint16_t x0, uint16_t y0, uint16_t r, gfx_color_t color) {
    // Midpoint Circle Algorithm 填充版本
    int16_t f = 1 - r;
    int16_t ddF_x = 1;
    int16_t ddF_y = -2 * r;
    int16_t x = 0;
    int16_t y = r;

    while (x <= y) {
        draw_line(x0 - x, y0 - y, x0 + x, y0 - y, color);
        draw_line(x0 - x, y0 + y, x0 + x, y0 + y, color);
        draw_line(x0 - y, y0 - x, x0 + y, y0 - x, color);
        draw_line(x0 - y, y0 + x, x0 + y, y0 + x, color);

        if (f >= 0) {
            y--;
            ddF_y += 2;
            f += ddF_y;
        }
        x++;
        ddF_x += 2;
        f += ddF_x;
    }
}

void draw_ascii_char(uint16_t x, uint16_t y, char c,  font_t font, gfx_color_t color) {
    if (c < font.start_char || c >= font.start_char + 95) {
        return; // 字符不在范围内
    }
    const uint8_t *char_data = &font.data[(c - font.start_char) * font.bytes_per_char];

    // 统一按列取模：每列占 ceil(height/8) 个字节，bit0 在顶部。
    uint8_t bytes_per_col = (font.height + 7U) / 8U;
    
    // 8x5 字体特殊处理：在中间空一个像素
    bool is_8x5_font = (font.width == 8 && font.height == 5);
    
    for (uint8_t col = 0; col < font.width; col++) {
        // 计算实际绘制 x 坐标
        uint16_t draw_x = x + col;
        if (is_8x5_font && col >= 4) {
            draw_x++;  // 从第 5 列开始，往右偏移 1 像素，形成中间空隙
        }
        
        for (uint8_t row = 0; row < font.height; row++) {
            uint8_t page = row / 8U;
            uint8_t bit = row % 8U;
            uint8_t col_data = char_data[col * bytes_per_col + page];
            if (col_data & (1U << bit)) {
                draw_pixel(draw_x, y + row, color);
            }
        }
    }
}

void draw_ascii_string(uint16_t x, uint16_t y, const char *str, font_t font, gfx_color_t color) {
    while (*str) {
        draw_ascii_char(x, y, *str, font, color);
        
        // 8x5 字体特殊处理：字符间距 = 8 + 1（中间空隙）= 9
        uint16_t advance = font.width;
        if (font.width == 8 && font.height == 5) {
            advance = 9;
        }
        
        x += advance;
        str++;
    }
}