/**
 * @file main.c
 * @author Kulib
 * @brief 程序入口
 * @version 0.1
 * @date 2026-04-24
 * */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "st7567_hw.h"
#include "display_gfx.h"
#include "font.h"
#include "font_manager.h"
#include "esp_log.h"

void app_main() {
    static const char *TAG = "main";
    font_ctx_t font_ctx = {0};
    uint32_t font_info_offset = 0;

    LCD_Init();
    gfx_clear_buffer(GFX_COLOR_BLACK);
    gfx_display();

    if (font_open(&font_ctx, 0x0) == ESP_OK) {
        font_info_offset = 0x0;
        ESP_LOGI(TAG, "font partition opened at info_offset=0x%lx", (unsigned long)font_info_offset);
    } else if (font_open(&font_ctx, 0x1000) == ESP_OK) {
        font_info_offset = 0x1000;
        ESP_LOGI(TAG, "font partition opened at info_offset=0x%lx", (unsigned long)font_info_offset);
    } else {
        ESP_LOGW(TAG, "font partition open failed at offsets 0x0/0x1000, fallback to ascii font");
    }

    while(1){
        // gfx_clear_buffer(GFX_COLOR_BLACK);
        // for(int i = 0; i < 95; i++){
        //     if(i % 8 == 0){
        //         //printf("clear buffer\n");
        //         gfx_clear_buffer(GFX_COLOR_BLACK);
        //         vTaskDelay(pdMS_TO_TICKS(100));
        //     }
        //     //printf("显示字符: %c\n", ' ' + i);
        //     draw_ascii_char(16 * (i % 8), (32 * ((i / 8) % 1)), ' ' + i, ascii_16X32, GFX_COLOR_WHITE);
        //     gfx_display();
        //     //vTaskDelay(pdMS_TO_TICKS(50)); // 每个字符显示后延时 50ms
        // }

        // gfx_clear_buffer(GFX_COLOR_BLACK);
        // for(int i = 0; i < 95; i++){
        //     if(i % 32 == 0){
        //         //printf("clear buffer\n");
        //         gfx_clear_buffer(GFX_COLOR_BLACK);
        //         vTaskDelay(pdMS_TO_TICKS(500));
        //     }
        //     //printf("显示字符: %c\n", ' ' + i);
        //     draw_ascii_char(8 * (i % 16), (16 * ((i / 16) % 2)), ' ' + i, ascii_8x16, GFX_COLOR_WHITE);
        //     gfx_display();
        //     // vTaskDelay(pdMS_TO_TICKS(50)); // 每个字符显示后延时 50ms
        // }
        
        gfx_clear_buffer(GFX_COLOR_BLACK);

        // 分区字库与 display_gfx 结合：先在 framebuffer 绘制，再统一 gfx_display 刷屏。
        if (font_ctx.part) {
            font_draw_utf8_string(&font_ctx, 0, 0, "我爱看利群与青岛", GFX_COLOR_WHITE);
            font_draw_utf8_string(&font_ctx, 0, 16, "安达与岛村也不错", GFX_COLOR_WHITE);
        }
        gfx_display();
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}   