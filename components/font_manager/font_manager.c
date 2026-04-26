/**
 * @file font_manager.c
 * @author Kulib
 * @brief 多字库字体管理 + 渲染
 * @date 2026-04-26
 * */

#include "font_manager.h"
#include <string.h>
#include "esp_log.h"

static const char *TAG = "font_mgr";

#define FONT_FLAG_UTF8_SORTED 0x04U

// ============================================================================
// 工具函数（内部使用）
// ============================================================================

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

static bool is_ascii_char(uint8_t byte)
{
    return byte >= 0x20U && byte <= 0x7EU;
}

static bool is_single_byte_ascii(const char *utf8_char)
{
    return utf8_char[0] != '\0' && utf8_char[1] == '\0' && 
           is_ascii_char((uint8_t)utf8_char[0]);
}

static int size_to_index(font_size_t size)
{
    switch (size) {
        case FONT_SIZE_SMALL:  return 0;
        case FONT_SIZE_NORMAL: return 1;
        case FONT_SIZE_LARGE:  return 2;
        default: return -1;
    }
}

static uint16_t get_missing_marker_side(font_size_t size)
{
    switch (size) {
        case FONT_SIZE_SMALL:
            return 5U;
        case FONT_SIZE_NORMAL:
            return 16U;
        case FONT_SIZE_LARGE:
            return 32U;
        default:
            return 16U;
    }
}

static uint16_t get_ascii_advance_for_size(const font_t *font, font_size_t size)
{
    if (!font) {
        return get_missing_marker_side(size);
    }

    if (size == FONT_SIZE_SMALL && font->width == 5U && font->height == 8U) {
        return 6U;
    }

    return font->width;
}


// ============================================================================
// 字库查找与读取
// ============================================================================

/**
 * @brief 打开字库（内部）
 */
static esp_err_t font_open_internal(font_ctx_t *ctx, uint32_t info_offset)
{
    if (!ctx) {
        return ESP_ERR_INVALID_ARG;
    }

    ctx->part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                         0x40, "font");
    if (!ctx->part) {
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t err = esp_partition_read(ctx->part, info_offset,
                                        &ctx->hdr, sizeof(font_header_t));
    if (err) {
        return err;
    }

    if (memcmp(ctx->hdr.head, "UTF8FNT\0", 8) != 0) {
        ESP_LOGD(TAG, "bad magic in font at offset 0x%lx", (unsigned long)info_offset);
        return ESP_ERR_INVALID_STATE;
    }

    if (ctx->hdr.index_entry_size != sizeof(font_index_entry_t)) {
        ESP_LOGE(TAG, "unsupported index entry size: %lu", 
                 (unsigned long)ctx->hdr.index_entry_size);
        return ESP_ERR_NOT_SUPPORTED;
    }

    if (!(ctx->hdr.flags & FONT_FLAG_UTF8_SORTED)) {
        ESP_LOGW(TAG, "index not sorted, will use linear search");
    }

    return ESP_OK;
}

/**
 * @brief 查找字库中的字形数据（二分或线性）
 */
static esp_err_t font_find_glyph_internal(const font_ctx_t *ctx,
                                          const char *utf8_char,
                                          uint8_t *out_buf,
                                          uint32_t buf_size)
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

    // 有序索引：二分查找
    if (ctx->hdr.flags & FONT_FLAG_UTF8_SORTED) {
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

    // 无序索引：线性查找
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


// ============================================================================
// 绘制原语
// ============================================================================

/**
 * @brief 按列主序绘制字形（UTF-8）
 */
static void draw_glyph_column_major(uint16_t x,
                                    uint16_t y,
                                    uint16_t width,
                                    uint16_t height,
                                    const uint8_t *glyph,
                                    gfx_color_t color)
{
    uint8_t bytes_per_col = (height + 7U) / 8U;

    for (uint16_t col = 0; col < width; col++) {
        for (uint16_t row = 0; row < height; row++) {
            uint16_t page = row / 8U;
            uint16_t bit = row % 8U;
            uint8_t v = glyph[col * bytes_per_col + page];

            if (v & (1U << bit)) {
                draw_pixel((uint16_t)(x + col), (uint16_t)(y + row), color);
            }
        }
    }
}

/**
 * @brief 绘制"缺字"标记（方框 + X）
 */
static void draw_missing_marker(uint16_t x,
                                uint16_t y,
                                uint16_t width,
                                uint16_t height,
                                gfx_color_t color)
{
    uint16_t margin = (width > 6U && height > 6U) ? 2U : 1U;

    // if (width <= (2U * margin + 1U) || height <= (2U * margin + 1U)) {
    //     // 空间不足：简单方框 + 对角线
    //     draw_rect(x, y, width, height, color);
    //     draw_line(x, y, (uint16_t)(x + width - 1U), (uint16_t)(y + height - 1U), color);
    //     draw_line((uint16_t)(x + width - 1U), y, x, (uint16_t)(y + height - 1U), color);
    //     return;
    // }

    // 空间充足：缩小方框 + 内部 X
    uint16_t bx = (uint16_t)(x + margin);
    uint16_t by = (uint16_t)(y + margin);
    uint16_t bw = (uint16_t)(width - 2U * margin);
    uint16_t bh = (uint16_t)(height - 2U * margin);

    draw_rect(bx, by, bw, bh, color);
    draw_line(bx, by, (uint16_t)(bx + bw - 1U), (uint16_t)(by + bh - 1U), color);
    draw_line((uint16_t)(bx + bw - 1U), by, bx, (uint16_t)(by + bh - 1U), color);
}

static void draw_missing_marker_centered(uint16_t x,
                                         uint16_t y,
                                         uint16_t cell_w,
                                         uint16_t cell_h,
                                         uint16_t marker_size,
                                         gfx_color_t color)
{
    if (cell_w == 0U || cell_h == 0U || marker_size == 0U) {
        return;
    }

    uint16_t side = marker_size;
    if (side > cell_w) {
        side = cell_w;
    }
    if (side > cell_h) {
        side = cell_h;
    }

    uint16_t dx = (uint16_t)((cell_w - side) / 2U);
    uint16_t dy = (uint16_t)((cell_h - side) / 2U);
    draw_missing_marker((uint16_t)(x + dx), (uint16_t)(y + dy), side, side, color);
}

// ============================================================================
// 公开 API
// ============================================================================

esp_err_t font_manager_init(font_manager_t *mgr)
{
    if (!mgr) {
        return ESP_ERR_INVALID_ARG;
    }

    memset(mgr, 0, sizeof(font_manager_t));
    return ESP_OK;
}

esp_err_t font_manager_register_ascii(font_manager_t *mgr,
                                      font_size_t size,
                                      const font_t *font)
{
    if (!mgr) {
        return ESP_ERR_INVALID_ARG;
    }

    int idx = size_to_index(size);
    if (idx < 0 || idx > 2) {
        return ESP_ERR_INVALID_ARG;
    }

    mgr->ascii[idx] = font;
    return ESP_OK;
}

esp_err_t font_manager_register_utf8(font_manager_t *mgr,
                                     font_size_t size,
                                     uint32_t info_offset)
{
    if (!mgr) {
        return ESP_ERR_INVALID_ARG;
    }

    int idx = size_to_index(size);
    if (idx < 0 || idx > 2) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t count = mgr->utf8_count[idx];
    if (count >= 4) {
        ESP_LOGE(TAG, "too many UTF-8 fonts for size %d (max 4)", size);
        return ESP_ERR_NO_MEM;
    }

    font_ctx_t *ctx = &mgr->utf8[idx][count];
    esp_err_t err = font_open_internal(ctx, info_offset);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "failed to open UTF-8 font at offset 0x%lx: %d",
                 (unsigned long)info_offset, err);
        return err;
    }

    mgr->utf8_count[idx]++;
    ESP_LOGI(TAG, "registered UTF-8 font (size=%d): %s (w=%d h=%d)",
             size, ctx->hdr.font_name, ctx->hdr.width, ctx->hdr.height);

    return ESP_OK;
}

esp_err_t font_manager_draw_char(const font_manager_t *mgr,
                                 const char *utf8_char,
                                 uint16_t x,
                                 uint16_t y,
                                 font_size_t size,
                                 gfx_color_t color)
{
    if (!mgr || !utf8_char) {
        return ESP_ERR_INVALID_ARG;
    }

    int idx = size_to_index(size);
    if (idx < 0 || idx > 2) {
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t len = utf8_char_len((uint8_t)utf8_char[0]);
    if (len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t marker_side = get_missing_marker_side(size);

    // 检查是否为 ASCII
    if (is_single_byte_ascii(utf8_char)) {
        if (mgr->ascii[idx]) {
            draw_ascii_char(x, y, utf8_char[0], *mgr->ascii[idx], color);
            return ESP_OK;
        }

        // ASCII 字体不可用：强制使用该字号缺字方框
        draw_missing_marker_centered(x, y, marker_side, marker_side, marker_side, color);
        return ESP_OK;
    }

    // 非 ASCII：在 UTF-8 字库中查找
    uint8_t count = mgr->utf8_count[idx];
    if (count == 0) {
        // 无可用字库：强制使用该字号缺字方框
        draw_missing_marker_centered(x, y, marker_side, marker_side, marker_side, color);
        return ESP_OK;
    }

    // 遍历该大小的所有 UTF-8 字库
    for (uint8_t i = 0; i < count; i++) {
        font_ctx_t *ctx = (font_ctx_t *)&mgr->utf8[idx][i];
        esp_err_t err = font_find_glyph_internal(ctx, utf8_char,
                                                 (uint8_t *)mgr->glyph_buffer,
                                                 sizeof(mgr->glyph_buffer));
        if (err == ESP_OK) {
            draw_glyph_column_major(x, y, ctx->hdr.width, ctx->hdr.height,
                                    (const uint8_t *)mgr->glyph_buffer, color);
            return ESP_OK;
        }

        if (err != ESP_ERR_NOT_FOUND) {
            ESP_LOGW(TAG, "font_find_glyph_internal failed: %d", err);
            // 继续尝试下一个字库
        }
    }

    // 所有字库都未找到：显示缺字标记
    uint16_t w = mgr->utf8[idx][0].hdr.width;
    uint16_t h = mgr->utf8[idx][0].hdr.height;
    uint16_t cell_w = (w > marker_side) ? w : marker_side;
    uint16_t cell_h = (h > marker_side) ? h : marker_side;
    draw_missing_marker_centered(x, y, cell_w, cell_h, marker_side, color);

    return ESP_OK;
}

esp_err_t font_manager_draw_string(const font_manager_t *mgr,
                                   const char *utf8_str,
                                   uint16_t x,
                                   uint16_t y,
                                   font_size_t size,
                                   gfx_color_t color)
{
    if (!mgr || !utf8_str) {
        return ESP_ERR_INVALID_ARG;
    }

    int idx = size_to_index(size);
    if (idx < 0 || idx > 2) {
        return ESP_ERR_INVALID_ARG;
    }

    uint16_t marker_side = get_missing_marker_side(size);

    const char *p = utf8_str;
    uint16_t cur_x = x;

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

        // 绘制单字符
        font_manager_draw_char(mgr, one, cur_x, y, size, color);

        // 计算推进宽度
        uint16_t advance = 0;
        if (is_single_byte_ascii(one)) {
            // ASCII 推进
            if (mgr->ascii[idx]) {
                advance = get_ascii_advance_for_size(mgr->ascii[idx], size);
            } else {
                advance = marker_side;
            }
        } else {
            // UTF-8 推进
            if (mgr->utf8_count[idx] > 0) {
                uint16_t glyph_w = mgr->utf8[idx][0].hdr.width;
                advance = (glyph_w > marker_side) ? glyph_w : marker_side;
            } else {
                advance = marker_side;
            }
        }

        cur_x = (uint16_t)(cur_x + advance);
        p += len;
    }

    return ESP_OK;
}