/**
 * @file main.c
 * @author Kulib
 * @brief 程序入口 - 完整的多字库混合渲染示例
 * @version 1.0
 * @date 2026-04-26
 * */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "st7567_hw.h"
#include "display_gfx.h"
#include "font.h"
#include "font_manager.h"
#include "esp_log.h"

static void draw_demo_scene(const font_manager_t *mgr)
{
    gfx_clear_buffer(GFX_COLOR_BLACK);

    // 外框
    draw_rect(0, 0, 128, 32, GFX_COLOR_WHITE);
    // 顶部标题栏
    fill_rect(1, 1, 126, 8, GFX_COLOR_WHITE);
    font_manager_draw_string(mgr, "    FONT DEMO", 4, 1, FONT_SIZE_SMALL, GFX_COLOR_BLACK);

    // 中部分割线与右侧图形区分隔
    draw_line(1, 10, 126, 10, GFX_COLOR_WHITE);
    draw_line(96, 10, 96, 30, GFX_COLOR_WHITE);

    // 左侧文本区：大/小字体混排
    font_manager_draw_string(mgr, "L", 0, 0, FONT_SIZE_LARGE, GFX_COLOR_INVERT);
    font_manager_draw_string(mgr, "  混排Mix", 0, 12, FONT_SIZE_NORMAL, GFX_COLOR_WHITE);
    font_manager_draw_string(mgr, "Small", 72, 12, FONT_SIZE_SMALL, GFX_COLOR_WHITE);
    font_manager_draw_string(mgr, "Font", 72, 20, FONT_SIZE_SMALL, GFX_COLOR_WHITE);
    // 右侧图形区
    draw_circle(112, 19, 7, GFX_COLOR_WHITE);
    draw_line(105, 19, 119, 19, GFX_COLOR_WHITE);
    draw_line(112, 12, 112, 26, GFX_COLOR_WHITE);
    draw_rect(101, 27, 22, 3, GFX_COLOR_WHITE);
}

void app_main() {
    static const char *TAG = "main";
    font_manager_t mgr;

    LCD_Init();
    gfx_clear_buffer(GFX_COLOR_BLACK);
    gfx_display();

    // 初始化字体管理器
    font_manager_init(&mgr);

    // 注册 ASCII 字体（小/中/大）
    font_manager_register_ascii(&mgr, FONT_SIZE_SMALL, &ascii_5x8);
    font_manager_register_ascii(&mgr, FONT_SIZE_NORMAL, &ascii_8x16);
    font_manager_register_ascii(&mgr, FONT_SIZE_LARGE, &ascii_16X32);

    // 注册 UTF-8 字库（当前只有 16x16 字库，用于 NORMAL 展示）
    esp_err_t err1 = font_manager_register_utf8(&mgr, FONT_SIZE_NORMAL, 0x0);
    esp_err_t err2 = font_manager_register_utf8(&mgr, FONT_SIZE_NORMAL, 0x1000);

    if (err1 != ESP_OK) {
        ESP_LOGW(TAG, "failed to register UTF-8 font at offset 0x0");
    }
    if (err2 != ESP_OK) {
        ESP_LOGW(TAG, "failed to register UTF-8 font at offset 0x1000");
    }

    ESP_LOGI(TAG, "font demo manager ready");

    while (1) {
        draw_demo_scene(&mgr);
        gfx_display();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}