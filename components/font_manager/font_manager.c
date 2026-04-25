/**
 * @file font_manager.c
 * @author Kulib
 * @brief 字体管理器实现
 * @version 0.1
 * @date 2026-04-25
 * */

#include "font_manager.h"
#include <string.h>
#include "esp_log.h"


static const char *TAG = "font";

#define FONT_FLAG_UTF8_SORTED 0x04U

static uint8_t utf8_char_len(uint8_t lead)
{
    if ((lead & 0x80U) == 0x00U) return 1;
    if ((lead & 0xE0U) == 0xC0U) return 2;
    if ((lead & 0xF0U) == 0xE0U) return 3;
    if ((lead & 0xF8U) == 0xF0U) return 4;
    return 0;
}

static esp_err_t make_utf8_key(const char *utf8_char, uint8_t key[4])
{
    if (!utf8_char || !key) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(key, 0, 4);
    uint8_t len = utf8_char_len((uint8_t)utf8_char[0]);
    if (len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    for (uint8_t i = 0; i < len; i++) {
        if (utf8_char[i] == '\0') {
            return ESP_ERR_INVALID_ARG;
        }
        key[i] = (uint8_t)utf8_char[i];
    }
    return ESP_OK;
}

static void draw_glyph_column_major(const font_ctx_t *ctx,
                                    uint16_t x,
                                    uint16_t y,
                                    const uint8_t *glyph,
                                    gfx_color_t color)
{
    uint8_t bytes_per_col = (ctx->hdr.height + 7U) / 8U;

    for (uint16_t col = 0; col < ctx->hdr.width; col++) {
        for (uint16_t row = 0; row < ctx->hdr.height; row++) {
            uint16_t page = row / 8U;
            uint16_t bit = row % 8U;
            uint8_t v = glyph[col * bytes_per_col + page];

            if (v & (1U << bit)) {
                draw_pixel((uint16_t)(x + col), (uint16_t)(y + row), color);
            }
        }
    }
}

static void draw_missing_glyph_marker(const font_ctx_t *ctx,
                                      uint16_t x,
                                      uint16_t y,
                                      gfx_color_t color)
{
    uint16_t w = ctx->hdr.width;
    uint16_t h = ctx->hdr.height;
    uint16_t margin = (w > 6U && h > 6U) ? 2U : 1U;

    if (w <= (2U * margin + 1U) || h <= (2U * margin + 1U)) {
        draw_rect(x, y, w, h, color);
        draw_line(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U), color);
        draw_line((uint16_t)(x + w - 1U), y, x, (uint16_t)(y + h - 1U), color);
        return;
    }

    uint16_t bx = (uint16_t)(x + margin);
    uint16_t by = (uint16_t)(y + margin);
    uint16_t bw = (uint16_t)(w - 2U * margin);
    uint16_t bh = (uint16_t)(h - 2U * margin);

    // 稍小方框 + 内部 X
    draw_rect(bx, by, bw, bh, color);
    draw_line(bx, by, (uint16_t)(bx + bw - 1U), (uint16_t)(by + bh - 1U), color);
    draw_line((uint16_t)(bx + bw - 1U), by, bx, (uint16_t)(by + bh - 1U), color);
}

// 打开字库，读取头部信息
esp_err_t font_open(font_ctx_t *ctx, uint32_t info_offset)
{
    if (!ctx) {
        return ESP_ERR_INVALID_ARG;
    }

    ctx->part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                         0x40, "font");
    if (!ctx->part) return ESP_ERR_NOT_FOUND;

    esp_err_t err = esp_partition_read(ctx->part, info_offset,
                                        &ctx->hdr, sizeof(font_header_t));
    if (err) return err;

    if (memcmp(ctx->hdr.head, "UTF8FNT\0", 8) != 0) {
        ESP_LOGD(TAG, "bad magic");
        return ESP_ERR_INVALID_STATE;
    }
    if (ctx->hdr.index_entry_size != sizeof(font_index_entry_t)) {
        ESP_LOGE(TAG, "unsupported index entry size: %lu", (unsigned long)ctx->hdr.index_entry_size);
        return ESP_ERR_NOT_SUPPORTED;
    }
    if (!(ctx->hdr.flags & FONT_FLAG_UTF8_SORTED)) {
        ESP_LOGW(TAG, "index not sorted, fallback to linear search");
    }
    return ESP_OK;
}

esp_err_t font_find_glyph(const font_ctx_t *ctx, const char *utf8_char,
                            uint8_t *out_buf, uint32_t buf_size)
{
    if (!ctx || !ctx->part || !utf8_char || !out_buf) {
        return ESP_ERR_INVALID_ARG;
    }
    if (buf_size < ctx->hdr.glyph_bytes) {
        return ESP_ERR_NO_MEM;
    }

    uint8_t needle[4] = {0};
    esp_err_t err = make_utf8_key(utf8_char, needle);
    if (err != ESP_OK) {
        return err;
    }

    font_index_entry_t entry;

    if (ctx->hdr.flags & FONT_FLAG_UTF8_SORTED) {
        // 二分查找
        int32_t left = 0;
        int32_t right = (int32_t)ctx->hdr.glyph_count - 1;

        while (left <= right) {
            int32_t mid = (left + right) / 2;
            uint32_t off = ctx->hdr.index_offset +
                           (uint32_t)mid * ctx->hdr.index_entry_size;

            err = esp_partition_read(ctx->part, off, &entry, sizeof(entry));
            if (err != ESP_OK) {
                return err;
            }

            int cmp = memcmp(needle, entry.utf8_key, 4);
            if (cmp == 0) {
                uint32_t bmp_off = ctx->hdr.bitmap_offset + entry.glyph_offset;
                return esp_partition_read(ctx->part, bmp_off,
                                           out_buf, ctx->hdr.glyph_bytes);
            }
            if (cmp < 0) {
                right = mid - 1;
            } else {
                left = mid + 1;
            }
        }
        return ESP_ERR_NOT_FOUND;
    }

    // 旧字库或未标记有序：线性查找，兼容性优先。
    for (uint32_t i = 0; i < ctx->hdr.glyph_count; i++) {
        uint32_t off = ctx->hdr.index_offset + i * ctx->hdr.index_entry_size;
        err = esp_partition_read(ctx->part, off, &entry, sizeof(entry));
        if (err != ESP_OK) {
            return err;
        }

        if (memcmp(needle, entry.utf8_key, 4) == 0) {
            uint32_t bmp_off = ctx->hdr.bitmap_offset + entry.glyph_offset;
            return esp_partition_read(ctx->part, bmp_off,
                                       out_buf, ctx->hdr.glyph_bytes);
        }
    }

    return ESP_ERR_NOT_FOUND;
}

esp_err_t font_draw_utf8_char(const font_ctx_t *ctx,
                              uint16_t x,
                              uint16_t y,
                              const char *utf8_char,
                              gfx_color_t color)
{
    if (!ctx || !utf8_char) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t glyph_buf[128];
    if (ctx->hdr.glyph_bytes > sizeof(glyph_buf)) {
        ESP_LOGE(TAG, "glyph bytes too large for stack buffer: %lu", (unsigned long)ctx->hdr.glyph_bytes);
        return ESP_ERR_NO_MEM;
    }

    esp_err_t err = font_find_glyph(ctx, utf8_char, glyph_buf, sizeof(glyph_buf));
    if (err != ESP_OK) {
        return err;
    }

    draw_glyph_column_major(ctx, x, y, glyph_buf, color);
    return ESP_OK;
}

esp_err_t font_draw_utf8_string(const font_ctx_t *ctx,
                                uint16_t x,
                                uint16_t y,
                                const char *utf8_str,
                                gfx_color_t color)
{
    if (!ctx || !utf8_str) {
        return ESP_ERR_INVALID_ARG;
    }

    const char *p = utf8_str;
    esp_err_t first_err = ESP_OK;
    while (*p) {
        uint8_t len = utf8_char_len((uint8_t)p[0]);
        if (len == 0) {
            return ESP_ERR_INVALID_ARG;
        }

        char one[5] = {0};
        for (uint8_t i = 0; i < len; i++) {
            if (p[i] == '\0') {
                return ESP_ERR_INVALID_ARG;
            }
            one[i] = p[i];
        }

        esp_err_t err = font_draw_utf8_char(ctx, x, y, one, color);
        if (err == ESP_ERR_NOT_FOUND) {
            // 缺字时绘制稍小方框 + X，避免整串回退到 FONT MISS。
            draw_missing_glyph_marker(ctx, x, y, color);
            if (first_err == ESP_OK) {
                first_err = err;
            }
        } else if (err != ESP_OK) {
            return err;
        }

        x = (uint16_t)(x + ctx->hdr.width);
        p += len;
    }

    return first_err;
}