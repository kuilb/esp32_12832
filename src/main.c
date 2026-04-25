/**
 * @file main.c
 * @author Kulib
 * @brief 程序入口
 * @version 0.1
 * @date 2026-04-24
 * */
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "st7567_driver/st7567_hw.h"
#include "display_gfx/display_gfx.h"
#include "display_gfx/font.h"

void app_main() {
    LCD_Init();
    gfx_clear_buffer(GFX_COLOR_BLACK);
    gfx_display();
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
        for(int i = 0; i < 95; i++){
            if(i % 84 == 0){
                //printf("clear buffer\n");
                gfx_display();
                vTaskDelay(pdMS_TO_TICKS(1000));
                gfx_clear_buffer(GFX_COLOR_BLACK);
            }
            if(i % 94 == 0){
                gfx_display();
                vTaskDelay(pdMS_TO_TICKS(1000));
                gfx_clear_buffer(GFX_COLOR_BLACK);
            }
            //printf("显示字符: %c\n", ' ' + i);
            draw_ascii_char(6 * (i % 21), (8 * ((i / 21) % 4)), ' ' + i, ascii_5x8, GFX_COLOR_WHITE);
            // gfx_display();
            // vTaskDelay(pdMS_TO_TICKS(50)); // 每个字符显示后延时 50ms
        }
    }
}   