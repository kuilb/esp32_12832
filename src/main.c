/**
 * @file main.c
 * @author Kulib
 * @brief 主程序入口
 * @version 0.1
 * @date 2026-04-24
 * */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "st7567_driver/st7567_hw.h"
#include "driver/gpio.h"
#include "gpio_define.h"

// 画空心矩形边框
static void draw_rect(int x0, int y0, int x1, int y1) {
    for (int x = x0; x <= x1; x++) {
        LCD_DrawPixel(x, y0);
        LCD_DrawPixel(x, y1);
    }
    for (int y = y0; y <= y1; y++) {
        LCD_DrawPixel(x0, y);
        LCD_DrawPixel(x1, y);
    }
}

void app_main() {
    LCD_Init();

    // 画测试图形：外框 + 内框 + 中心十字
    LCD_Clear();
    draw_rect(0, 0, 127, 31);
    draw_rect(4, 4, 123, 27);
    for (int i = 0; i < 128; i++) { LCD_DrawPixel(i, 15); LCD_DrawPixel(i, 16); }
    for (int y = 0; y < 32; y++) { LCD_DrawPixel(63, y); LCD_DrawPixel(64, y); }
    LCD_DrawStr(2, 8, "3.0/9");
    while (1) { vTaskDelay(pdMS_TO_TICKS(10000)); }
}