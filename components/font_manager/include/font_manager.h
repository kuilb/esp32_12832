/**
 * @file font_manager.h
 * @author Kulib
 * @brief 声明字体管理相关的接口
 * @version 0.1
 * @date 2026-04-25
 * */

#ifndef FONT_MANAGER_H
#define FONT_MANAGER_H

#include <stdint.h>
#include "esp_partition.h"
#include "display_gfx.h"

// 字库文件格式定义
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

// 字形索引项定义
typedef struct __attribute__((packed)) {
    uint8_t  utf8_key[4];
    uint32_t glyph_offset;      // 相对 bitmap_offset 的偏移
} font_index_entry_t;

// 字体上下文结构体
typedef struct {
    const esp_partition_t *part;
    font_header_t          hdr;
} font_ctx_t;

/**
 * @brief 打开字库，读取头部信息
 * @param ctx 字体上下文指针，成功时会填充相关信息
 * @param info_offset 字库信息在分区中的偏移位置
 * @return ESP_OK 成功，其他值表示错误
 */
esp_err_t font_open(font_ctx_t *ctx, uint32_t info_offset);

/**
 * @brief 查找指定 UTF-8 字符的字形数据
 * @param ctx 已打开的字体上下文
 * @param utf8_char 待查找的 UTF-8 字符串（最长4字节，包含结尾0）
 * @param out_buf 输出缓冲区，成功时会填充字形数据
 * @param buf_size 输出缓冲区大小，必须至少为 hdr.glyph_bytes
 * @return ESP_OK 成功，ESP_ERR_NOT_FOUND 未找到，ESP_ERR_NO_MEM 输出缓冲区不足，其他值表示错误
 */
esp_err_t font_find_glyph(const font_ctx_t *ctx, const char *utf8_char,
                           uint8_t *out_buf, uint32_t buf_size);

/**
 * @brief 在 display_gfx 帧缓冲中绘制一个 UTF-8 字符（按字库取模格式）
 * @param ctx 已打开的字体上下文
 * @param x 左上角 x
 * @param y 左上角 y
 * @param utf8_char UTF-8 字符，长度 1~4 字节
 * @param color 绘制颜色
 * @return ESP_OK 成功，其他值表示查字失败或参数错误
 */
esp_err_t font_draw_utf8_char(const font_ctx_t *ctx,
                              uint16_t x,
                              uint16_t y,
                              const char *utf8_char,
                              gfx_color_t color);

/**
 * @brief 在 display_gfx 帧缓冲中绘制 UTF-8 字符串（逐字符）
 * @param ctx 已打开的字体上下文
 * @param x 左上角 x
 * @param y 左上角 y
 * @param utf8_str UTF-8 字符串
 * @param color 绘制颜色
 * @return ESP_OK 成功，遇到任意字符绘制失败则返回对应错误
 */
esp_err_t font_draw_utf8_string(const font_ctx_t *ctx,
                                uint16_t x,
                                uint16_t y,
                                const char *utf8_str,
                                gfx_color_t color);

#endif // FONT_MANAGER_H