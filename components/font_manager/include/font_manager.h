/**
 * @file font_manager.h
 * @author Kulib
 * @brief 多字库字体管理
 * @date 2026-04-26
 * */

#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#include <stddef.h>
#include <stdint.h>
#include "esp_partition.h"
#include "display_gfx.h"
#include "font.h"

/**
 * 字体大小枚举
 */
typedef enum {
    FONT_SIZE_SMALL  = 8,   // 8x16 等小字体
    FONT_SIZE_NORMAL = 16,  // 16x16 等标准字体
    FONT_SIZE_LARGE  = 32,  // 32x32 等大字体
} font_size_t;

/**
 * 字库文件格式定义
 */
typedef struct __attribute__((packed)) {
    char head[8];               // "UTF8FNT\0" 指定字库头
    uint16_t version;           // 字库版本
    uint16_t header_size;       // 头部长度（当前应为 64）
    uint16_t width;             // 字宽
    uint16_t height;            // 字高
    uint32_t glyph_bytes;       // 单字字模字节数
    uint32_t glyph_count;       // 字形数量
    uint32_t index_entry_size;  // 索引项大小（字节）
    uint32_t index_offset;      // 索引数据偏移（相对于文件开始）
    uint32_t index_bytes;       // 索引数据总字节数
    uint32_t bitmap_offset;     // 字形数据偏移（相对于文件开始）
    uint32_t bitmap_bytes;      // 字形数据总字节数
    uint32_t flags;             // 字库标志位
    char     font_name[16];     // 字体名称（ASCII，最长15字节）
} font_header_t;

/**
 * 字形索引项定义
 */
typedef struct __attribute__((packed)) {
    uint8_t  utf8_key[4];
    uint32_t glyph_offset;      // 相对 bitmap_offset 的偏移
} font_index_entry_t;

/**
 * 单个字库上下文
 */
typedef struct {
    const esp_partition_t *part;
    font_header_t          hdr;
} font_ctx_t;

/**
 * 字体管理器上下文（核心）
 * 支持多个大小的 ASCII 字体 + 多个 UTF-8 字库
 */
typedef struct {
    // ASCII 字体映射（按大小）
    const font_t *ascii[3];     // [0]=SMALL, [1]=NORMAL, [2]=LARGE
    
    // UTF-8 字库映射（每个大小最多4个字库）
    font_ctx_t utf8[3][4];      // [size][index]
    uint8_t    utf8_count[3];   // 各大小的字库数量
    
    // 内部缓冲区
    uint8_t glyph_buffer[512];  // 单字形数据缓冲
} font_manager_t;

/**
 * @brief 初始化字体管理器
 * @param mgr 管理器指针
 * @return ESP_OK 成功
 */
esp_err_t font_manager_init(font_manager_t *mgr);

/**
 * @brief 注册 ASCII 字体到指定大小位置
 * @param mgr 管理器指针
 * @param size 字体大小（FONT_SIZE_SMALL/NORMAL/LARGE）
 * @param font ASCII 字体指针
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 大小无效
 */
esp_err_t font_manager_register_ascii(font_manager_t *mgr, font_size_t size, const font_t *font);

/**
 * @brief 注册 UTF-8 字库到指定大小
 * @param mgr 管理器指针
 * @param size 字体大小（FONT_SIZE_SMALL/NORMAL/LARGE）
 * @param info_offset 字库头部在 flash 中的偏移
 * @return ESP_OK 成功，ESP_ERR_INVALID_ARG 大小无效或超过上限，其他表示打开失败
 */
esp_err_t font_manager_register_utf8(font_manager_t *mgr, font_size_t size, uint32_t info_offset);

/**
 * @brief 单字符渲染（支持 ASCII + UTF-8 混合查询）
 * @details 若指定大小的字体缺少该字符，则显示该大小的"方框X"
 * @param mgr 管理器指针
 * @param utf8_char 单个 UTF-8 字符（1-4 字节）
 * @param x 左上角 x
 * @param y 左上角 y
 * @param size 字体大小
 * @param color 绘制颜色
 * @return ESP_OK 成功（即使是缺字也返回 OK），其他值表示参数错误或底层故障
 */
esp_err_t font_manager_draw_char(const font_manager_t *mgr,
                                 const char *utf8_char,
                                 uint16_t x,
                                 uint16_t y,
                                 font_size_t size,
                                 gfx_color_t color);

/**
 * @brief 字符串渲染（支持 ASCII + UTF-8 混合）
 * @details 按各字符实际宽度推进，缺字显示对应大小的"方框X"
 * @param mgr 管理器指针
 * @param utf8_str UTF-8 字符串
 * @param x 左上角 x
 * @param y 左上角 y
 * @param size 字体大小
 * @param color 绘制颜色
 * @return ESP_OK 全部成功；若有缺字但均已用"方框X"填充，仍返回 ESP_OK；其他值表示参数错误
 */
esp_err_t font_manager_draw_string(const font_manager_t *mgr,
                                   const char *utf8_str,
                                   uint16_t x,
                                   uint16_t y,
                                   font_size_t size,
                                   gfx_color_t color);

#endif // FONT_MANAGER_H