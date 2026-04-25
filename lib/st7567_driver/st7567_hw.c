/**
 * @file st7567_hw.c
 * @author Kulib
 * @brief 定义 ST7567 (12832) 屏幕的驱动接口
 * @version 0.1
 * @date 2026-04-22
 * */

#include "st7567_driver/st7567_hw.h"

#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_rom_sys.h"   // esp_rom_delay_us()

typedef struct {
    uint8_t seg_remap;      // 列反向 0:普通, 1:反向
    uint8_t com_reverse;    // 行反向 0:普通, 1:反向
    uint8_t bias_1_7;       // 偏置比选择 0:1/9,   1:1/7
    uint8_t reg_ratio;      // 电阻调节比率 (对比度相关) 0x00~0x07
    uint8_t ev_level;       // 驱动电压等级 (对比度相关) 0x00~0x3F
    uint8_t col_offset;     // 列起始偏移
    uint8_t inverse;        // 反显控制 0:正常,  1:反显
} lcd_cfg_t;

static const lcd_cfg_t lcd_cfg = {
    .seg_remap    = LCD_SEG_REMAP,
    .com_reverse  = LCD_COM_REVERSE,
    .bias_1_7     = LCD_BIAS_1_7,
    .reg_ratio    = LCD_REG_RATIO,
    .ev_level     = LCD_EV_LEVEL,
    .col_offset   = LCD_COL_OFFSET,
    .inverse      = LCD_INVERSE,
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

static void _lcd_apply_config(const lcd_cfg_t *cfg);

static inline void _lcd_wr_strobe(void) {
    // ST7567 /WR 最小脉宽 60ns
    // 数据建立时间
    gpio_set_level(GPIO_WR, 0);
    // /WR 低电平保持
    gpio_set_level(GPIO_WR, 1);
    // 数据保持时间
}

// 准备 8 bit 数据到总线上
static inline void _lcd_set_data_bus(uint8_t value) {
    gpio_set_level(GPIO_D0, (value >> 0) & 0x01);
    gpio_set_level(GPIO_D1, (value >> 1) & 0x01);
    gpio_set_level(GPIO_D2, (value >> 2) & 0x01);
    gpio_set_level(GPIO_D3, (value >> 3) & 0x01);
    gpio_set_level(GPIO_D4, (value >> 4) & 0x01);
    gpio_set_level(GPIO_D5, (value >> 5) & 0x01);
    gpio_set_level(GPIO_D6, (value >> 6) & 0x01);
    gpio_set_level(GPIO_D7, (value >> 7) & 0x01);
}

static void _lcd_write_cmd(uint8_t cmd) {
    gpio_set_level(GPIO_CS, 0);
    gpio_set_level(GPIO_A0, 0);
    _lcd_set_data_bus(cmd);
    _lcd_wr_strobe();
    gpio_set_level(GPIO_CS, 1);
}

static void _lcd_write_data(uint8_t data) {
    gpio_set_level(GPIO_CS, 0);
    gpio_set_level(GPIO_A0, 1);
    _lcd_set_data_bus(data);
    _lcd_wr_strobe();
    gpio_set_level(GPIO_CS, 1);
}

// 设置页地址和列地址，准备写入数据
static void lcd_set_page_col(uint8_t page, uint8_t col, uint8_t s_col_offset) {
    uint8_t abs_col = col + s_col_offset; // 列偏移
    _lcd_write_cmd(CMD_SET_PAGE    | (page    & 0x0F));
    _lcd_write_cmd(CMD_SET_COL_MSB | ((abs_col >> 4) & 0x0F));  // 前 4 位
    _lcd_write_cmd(CMD_SET_COL_LSB | (abs_col & 0x0F));// 后 4 位
}

void LCD_flush_buffer(uint8_t s_framebuffer[LCD_PAGES][LCD_WIDTH], uint8_t s_col_offset) {
    // 按页写入
    for (uint8_t page = 0; page < LCD_PAGES; page++) {
        lcd_set_page_col(page, 0, s_col_offset);
        for (uint16_t x = 0; x < LCD_WIDTH; x++) {
            _lcd_write_data(s_framebuffer[page][x]);
        }
    }
}

void LCD_Clear() {
    // 按页写入全 0
    for (uint8_t page = 0; page < LCD_PAGES; page++) {
        lcd_set_page_col(page, 0, 0);
        for (uint16_t x = 0; x < LCD_WIDTH; x++) {
            _lcd_write_data(0x00);
        }
    }
}

static void _lcd_apply_config(const lcd_cfg_t *cfg) {
    // 按数据手册顺序初始化 ST7567（8080 并口）
    // 设置液晶驱动参数和地址反向
    _lcd_write_cmd(CMD_BIAS_CTRL | (cfg->bias_1_7 ? 0x01 : 0x00));  // (11) Bias 1/9 or 1/7
    _lcd_write_cmd(CMD_SEG_DIR   | (cfg->seg_remap ? 0x01 : 0x00)); // (8)  SEG direction
    _lcd_write_cmd(CMD_COM_DIR   | (cfg->com_reverse ? 0x08 : 0x00)); // (15) COM direction

    // 对比度设置
    _lcd_write_cmd(CMD_POWER_CTRL | 0x07); // (16) 开启内部电荷泵
    vTaskDelay(pdMS_TO_TICKS(150));       // 等待内部升压电路将 V0 充至 LCD 驱动电压
    _lcd_write_cmd(CMD_REG_RATIO  | (cfg->reg_ratio & 0x07)); // (17) V0 电阻比
    _lcd_write_cmd(CMD_SET_EV_MODE);                          // (18) EV 双字节指令第一字节
    _lcd_write_cmd(cfg->ev_level & 0x3F);                     //      EV 值

    _lcd_write_cmd(CMD_SET_START_LINE | 0x00);                       // (2)  起始行 0
    _lcd_write_cmd(CMD_ALL_PIXEL_CTRL);                              // (10) 正常显示 DDRAM
    _lcd_write_cmd(CMD_INVERSE_CTRL   | (cfg->inverse ? 0x01 : 0x00));       // (9) 反显控制

    LCD_Clear(); // 开显示前先清 DDRAM

    // 开启显示
    _lcd_write_cmd(CMD_DISPLAY_CTRL | 0x01); // (1)  Display ON
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

    _lcd_apply_config(&lcd_cfg);

    LCD_Clear();
}