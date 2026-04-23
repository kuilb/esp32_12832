/**
 * @file st7567_hw.c
 * @author Kulib
 * @brief 定义 ST7567 (12832) 屏幕的驱动接口
 * @version 0.1
 * @date 2026-04-22
 * */

#include "st7567_driver/st7567_hw.h"

#include <string.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"   // esp_rom_delay_us()

// 12832 屏幕，使用 Page 0-3
#define LCD_WIDTH   128
#define LCD_HEIGHT  32
#define LCD_PAGES   (LCD_HEIGHT / 8)

// 按需切换方向和对比度
#define LCD_SEG_REMAP        0   // 0:A0 普通, 1:A1 反向
#define LCD_COM_REVERSE      1   // 0:C0 普通, 1:C8 反向
#define LCD_BIAS_1_7         0   // 0:A2(1/9), 1:A3(1/7)
#define LCD_REG_RATIO        0x00  // RR=3.0
#define LCD_EV_LEVEL         0x2C  // 对比度 EV=0x2C

// 帧缓冲区，每页 128 字节，4 页共 512 字节
static uint8_t s_framebuffer[LCD_PAGES][LCD_WIDTH];
static uint8_t s_col_offset = 0; // ST7567 列寄存器 132 列，128 宽屏可能有 0 或 4 的偏移

typedef struct {
    uint8_t seg_remap;      // 列反向 0:普通, 1:反向
    uint8_t com_reverse;    // 行反向 0:普通, 1:反向
    uint8_t bias_1_7;       // 偏置比选择 0:1/9,   1:1/7
    uint8_t reg_ratio;      // 电阻调节比率 (对比度相关) 0x00~0x07
    uint8_t ev_level;       // 驱动电压等级 (对比度相关) 0x00~0x3F
    uint8_t col_offset;     // 列起始偏移（通常 0 或 4）
    uint8_t inverse;        // 反显控制 0:正常,  1:反显
} lcd_cfg_t;

static const lcd_cfg_t s_default_cfg = {
    .seg_remap    = LCD_SEG_REMAP,
    .com_reverse  = LCD_COM_REVERSE,
    .bias_1_7     = LCD_BIAS_1_7,
    .reg_ratio    = LCD_REG_RATIO,
    .ev_level     = LCD_EV_LEVEL,
    .col_offset   = 0,
    .inverse      = 0,
};

// 预设命令对应数据位，大端序
enum {
    CMD_DISPLAY_CTRL      = 0b10101110, // 屏幕显示关闭, +1 => ON
    CMD_SET_START_LINE    = 0b01000000, // 行偏移量 & +0~63
    CMD_SET_PAGE          = 0b10110000, // 页数 & +0~3
    CMD_SET_COL_MSB       = 0b00010000, // 行地址高 4 位
    CMD_SET_COL_LSB       = 0b00000000, // 行地址低 4 位
    CMD_SEG_DIR           = 0b10100000, // +1 => 列反向
    CMD_INVERSE_CTRL      = 0b10100110, // +1 => 反显
    CMD_ALL_PIXEL_CTRL    = 0b10100100, // +1 => 像素全亮
    CMD_BIAS_CTRL         = 0b10100010, // +1 => 1/7
    CMD_SOFT_RESET        = 0b11100010, // 软件复位
    CMD_COM_DIR           = 0b11000000, // +0x08 => 反向
    CMD_POWER_CTRL        = 0b00101000, // +VB|VR|VF
    CMD_REG_RATIO         = 0b00100000, // +RRR
    CMD_SET_EV_MODE       = 0b10000001, // 设置对比度，需跟一个字节 EV
};

static void lcd_apply_config(const lcd_cfg_t *cfg);

static inline void lcd_wr_strobe(void) {
    // ST7567 /WR 最小脉宽 60ns；esp_rom_delay_us(1) 约 1µs，远超要求
    esp_rom_delay_us(1);            // 数据建立时间
    gpio_set_level(GPIO_WR, 0);
    esp_rom_delay_us(1);            // /WR 低电平保持
    gpio_set_level(GPIO_WR, 1);
    esp_rom_delay_us(1);            // 数据保持时间
}

static inline void lcd_set_data_bus(uint8_t value) {
    gpio_set_level(GPIO_D0, (value >> 0) & 0x01);
    gpio_set_level(GPIO_D1, (value >> 1) & 0x01);
    gpio_set_level(GPIO_D2, (value >> 2) & 0x01);
    gpio_set_level(GPIO_D3, (value >> 3) & 0x01);
    gpio_set_level(GPIO_D4, (value >> 4) & 0x01);
    gpio_set_level(GPIO_D5, (value >> 5) & 0x01);
    gpio_set_level(GPIO_D6, (value >> 6) & 0x01);
    gpio_set_level(GPIO_D7, (value >> 7) & 0x01);
}

static void lcd_write_cmd(uint8_t cmd) {
    gpio_set_level(GPIO_CS, 0);
    gpio_set_level(GPIO_A0, 0);
    lcd_set_data_bus(cmd);
    lcd_wr_strobe();
    gpio_set_level(GPIO_CS, 1);
}

static void lcd_write_data(uint8_t data) {
    gpio_set_level(GPIO_CS, 0);
    gpio_set_level(GPIO_A0, 1);
    lcd_set_data_bus(data);
    lcd_wr_strobe();
    gpio_set_level(GPIO_CS, 1);
}

static void lcd_set_page_col(uint8_t page, uint8_t col) {
    uint8_t abs_col = col + s_col_offset; // 加上列偏移，适配 132 列控制器
    lcd_write_cmd(CMD_SET_PAGE    | (page    & 0x0F));
    lcd_write_cmd(CMD_SET_COL_MSB | ((abs_col >> 4) & 0x0F));
    lcd_write_cmd(CMD_SET_COL_LSB | (abs_col & 0x0F));
}

static void lcd_flush(void) {
    for (uint8_t page = 0; page < LCD_PAGES; page++) {
        lcd_set_page_col(page, 0);
        for (uint16_t x = 0; x < LCD_WIDTH; x++) {
            lcd_write_data(s_framebuffer[page][x]);
        }
    }
}

void LCD_Clear() {
    memset(s_framebuffer, 0, sizeof(s_framebuffer));
    lcd_flush();
}

void LCD_DrawPixel(int x, int y) {
    if (x < 0 || x >= LCD_WIDTH || y < 0 || y >= LCD_HEIGHT) {
        return;
    }
    uint8_t page = (uint8_t)(y >> 3);
    uint8_t bit = (uint8_t)(1U << (y & 0x07));
    s_framebuffer[page][x] |= bit;
    lcd_set_page_col(page, (uint8_t)x);
    lcd_write_data(s_framebuffer[page][x]);
}

// 5x8 字体，列主序（每字节为一列像素， bit0=顶行）
// 支持字符：' ' '.' '0'-'9' '/'
static const uint8_t s_font5x8[][5] = {
    {0x00, 0x00, 0x00, 0x00, 0x00}, // ' '
    {0x00, 0x60, 0x60, 0x00, 0x00}, // '.'
    {0x3E, 0x51, 0x49, 0x45, 0x3E}, // '0'
    {0x00, 0x42, 0x7F, 0x40, 0x00}, // '1'
    {0x62, 0x51, 0x49, 0x49, 0x46}, // '2'
    {0x22, 0x41, 0x49, 0x49, 0x36}, // '3'
    {0x18, 0x14, 0x12, 0x7F, 0x10}, // '4'
    {0x27, 0x45, 0x45, 0x45, 0x39}, // '5'
    {0x3C, 0x4A, 0x49, 0x49, 0x31}, // '6'
    {0x01, 0x71, 0x09, 0x05, 0x03}, // '7'
    {0x36, 0x49, 0x49, 0x49, 0x36}, // '8'
    {0x06, 0x49, 0x49, 0x29, 0x1E}, // '9'
    {0x30, 0x18, 0x0C, 0x06, 0x03}, // '/'
};

static int font_char_index(char c) {
    if (c == '.') return 1;
    if (c >= '0' && c <= '9') return c - '0' + 2;
    if (c == '/') return 12;
    return 0; // space / unknown
}

void LCD_DrawChar(int x, int y, char c) {
    if (x < 0 || x + 5 > LCD_WIDTH || y < 0 || y + 8 > LCD_HEIGHT) return;
    if (y % 8 != 0) return;
    uint8_t page = (uint8_t)(y / 8);
    const uint8_t *glyph = s_font5x8[font_char_index(c)];
    for (int col = 0; col < 5; col++) {
        s_framebuffer[page][x + col] |= glyph[col];
    }
    lcd_set_page_col(page, (uint8_t)x);
    for (int col = 0; col < 5; col++) {
        lcd_write_data(s_framebuffer[page][x + col]);
    }
}

void LCD_DrawStr(int x, int y, const char *s) {
    while (*s && x + 5 <= LCD_WIDTH) {
        LCD_DrawChar(x, y, *s++);
        x += 6;
    }
}

static void lcd_apply_config(const lcd_cfg_t *cfg) {
    // 按数据手册顺序初始化 ST7567（8080 并口）
    lcd_write_cmd(CMD_BIAS_CTRL | (cfg->bias_1_7 ? 0x01 : 0x00));  // (11) Bias 1/9 or 1/7
    lcd_write_cmd(CMD_SEG_DIR   | (cfg->seg_remap ? 0x01 : 0x00)); // (8)  SEG direction
    lcd_write_cmd(CMD_COM_DIR   | (cfg->com_reverse ? 0x08 : 0x00)); // (15) COM direction

    // 对比度（不发 0xF8 升压倍率命令，保持芯片上电默认 4X；厂商参考代码同样不发此命令）
    lcd_write_cmd(CMD_POWER_CTRL | 0x07); // (16) 电源全开：Booster+Regulator+Follower
    vTaskDelay(pdMS_TO_TICKS(150));       // 等待内部升压电路将 V0 充至 LCD 驱动电压
    lcd_write_cmd(CMD_REG_RATIO  | (cfg->reg_ratio & 0x07)); // (17) V0 电阻比
    lcd_write_cmd(CMD_SET_EV_MODE);                          // (18) EV 双字节指令第一字节
    lcd_write_cmd(cfg->ev_level & 0x3F);                     //      EV 值

    lcd_write_cmd(CMD_SET_START_LINE | 0x00);                       // (2)  起始行 0
    lcd_write_cmd(CMD_ALL_PIXEL_CTRL);                              // (10) 正常显示 DDRAM
    lcd_write_cmd(CMD_INVERSE_CTRL   | (cfg->inverse ? 0x01 : 0x00));       // (9)

    s_col_offset = cfg->col_offset; // 在 Clear 之前设置偏移，flush 时使用
    LCD_Clear(); // 按手册建议：开显示前先清 DDRAM

    lcd_write_cmd(CMD_DISPLAY_CTRL | 0x01); // (1)  Display ON
}

void LCD_Init() {
    // 配置 GPIO 引脚
    const uint64_t pin_mask =
        (1ULL << GPIO_CS) |
        (1ULL << GPIO_RES) |
        (1ULL << GPIO_A0) |
        (1ULL << GPIO_WR) |
        (1ULL << GPIO_RD) |
        (1ULL << GPIO_D0) |
        (1ULL << GPIO_D1) |
        (1ULL << GPIO_D2) |
        (1ULL << GPIO_D3) |
        (1ULL << GPIO_D4) |
        (1ULL << GPIO_D5) |
        (1ULL << GPIO_D6) |
        (1ULL << GPIO_D7);

    // 通过结构体配置引脚状态
    gpio_config_t io_conf = {
        .pin_bit_mask = pin_mask,   // 选择所有相关引脚
        .mode = GPIO_MODE_OUTPUT,   // 全部设置为输出模式
        .pull_up_en = GPIO_PULLUP_DISABLE,  // 不使用上拉
        .pull_down_en = GPIO_PULLDOWN_DISABLE, // 不使用下拉
        .intr_type = GPIO_INTR_DISABLE,// 不使用中断
    };
    gpio_config(&io_conf);

    // 空闲电平
    gpio_set_level(GPIO_CS, 1);
    gpio_set_level(GPIO_WR, 1);
    gpio_set_level(GPIO_RD, 1);
    gpio_set_level(GPIO_A0, 1);

    // 硬复位序列
    gpio_set_level(GPIO_RES, 1);
    vTaskDelay(pdMS_TO_TICKS(2));
    gpio_set_level(GPIO_RES, 0);
    vTaskDelay(pdMS_TO_TICKS(2));
    gpio_set_level(GPIO_RES, 1);
    vTaskDelay(pdMS_TO_TICKS(2));

    lcd_apply_config(&s_default_cfg);

    LCD_Clear();
}