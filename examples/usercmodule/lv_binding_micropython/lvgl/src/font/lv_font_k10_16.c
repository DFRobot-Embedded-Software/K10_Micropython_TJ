/*******************************************************************************
 * K10 Custom Font - Dynamic Font from GT30L24A3W Chip
 * Size: 16 px (ASCII_8X16 format)
 * All font data is dynamically loaded from external font chip
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "../../lvgl.h"
#endif

#ifndef LV_FONT_K10_16
    #define LV_FONT_K10_16 1
#endif

#if LV_FONT_K10_16

#include <string.h>
#include <stdint.h>
#include "k10_font_chip.h"
#include "py/runtime.h"
#include "py/mphal.h"
#include "../draw/lv_draw_buf.h"
#include "lv_font_fmt_txt.h"

/*-----------------
 *  FONT DIMENSIONS
 *----------------*/

// Font dimensions for ASCII_8X16
#define K10_ASCII_WIDTH        8
#define K10_ASCII_HEIGHT       16
#define K10_ASCII_BYTES        ((K10_ASCII_WIDTH * K10_ASCII_HEIGHT) / 8)  // 16 bytes

// Font dimensions for Chinese characters (12x24)
#define K10_CHINESE_WIDTH      12
#define K10_CHINESE_HEIGHT     24
#define K10_CHINESE_BYTES      ((K10_CHINESE_WIDTH * K10_CHINESE_HEIGHT) / 8)  // 36 bytes

#define K10_FONT_BPP           1

// ASCII character range
#define K10_ASCII_START        32   // Space
#define K10_ASCII_END          126  // ~
#define K10_ASCII_COUNT        (K10_ASCII_END - K10_ASCII_START + 1)

// Chinese character range (GBK encoding)
#define K10_GBK_START          0xA1A1  // First GBK character
#define K10_GBK_END            0xFEFE  // Last GBK character
#define K10_GBK_COUNT          6768    // Approximate GBK character count

// Total character count (ASCII + Chinese)
#define K10_TOTAL_CHARS        (K10_ASCII_COUNT + K10_GBK_COUNT)

// Font chip initialization flag
static int k10_font_initialized = 0;

// Initialize font chip
static int k10_font_init(void) {
    if (!k10_font_initialized) {
        k10_font_initialized = GT_Font_Init();
    }
    return k10_font_initialized;
}

/*-----------------
 *  DYNAMIC FONT CALLBACKS
 *----------------*/

// Buffer for dynamic font data (1bpp from chip)
static uint8_t k10_font_buffer[K10_ASCII_BYTES];

// Dynamic glyph descriptor callback
static bool k10_get_glyph_dsc(const lv_font_t * font, lv_font_glyph_dsc_t * dsc_out, uint32_t unicode_letter, uint32_t unicode_letter_next) {
    // Safety check
    if (!font || !dsc_out) {
        return false;
    }
    
    // Initialize font chip if needed
    if (!k10_font_init()) {
        return false;
    }
    
    // Check if it's a printable ASCII character
    if (unicode_letter < 32 || unicode_letter > 126) {
        return false;  // Character not supported
    }
    
    // Set glyph descriptor for ASCII 8x16 format
    dsc_out->resolved_font = font;       // Set the resolved font
    dsc_out->box_h = K10_ASCII_HEIGHT;   // Height of the glyph bitmap (in pixels)
    dsc_out->box_w = K10_ASCII_WIDTH;    // Width of the glyph bitmap (in pixels)
    dsc_out->adv_w = 8;                  // Letter spacing (8 pixels for 8x16)
    dsc_out->ofs_x = 0;                  // X offset of the glyph bitmap (in pixels)
    dsc_out->ofs_y = 0;                  // Y offset of the glyph bitmap (in pixels), relative to the baseline
    dsc_out->format = LV_FONT_GLYPH_FORMAT_A1;  // Original format is 1bpp
    dsc_out->is_placeholder = false;
    dsc_out->req_raw_bitmap = 0;         // We'll do the conversion ourselves
    dsc_out->gid.index = unicode_letter; // Store the unicode character as glyph ID
    dsc_out->entry = NULL;               // No cache entry for dynamic fonts
    
    return true;  // Character found
}

// Dynamic glyph bitmap callback
static const void * k10_get_glyph_bitmap(lv_font_glyph_dsc_t * g_dsc, lv_draw_buf_t * draw_buf) {
    // Safety check
    if (!g_dsc) {
        return NULL;
    }
    
    uint32_t unicode_letter = g_dsc->gid.index;
    
    // Check if unicode_letter is valid
    if (unicode_letter < 32 || unicode_letter > 126) {
        return NULL;
    }
    
    // Initialize font chip if needed
    if (!k10_font_init()) {
        return NULL;
    }
    
    // Get font data from K10 chip
    if (ASCII_GetData((unsigned char)unicode_letter, ASCII_8X16, k10_font_buffer)) {
        // Use the generic conversion function from fmt_txt
        if (draw_buf && draw_buf->data) {
            const uint8_t * bitmap_in = k10_font_buffer;
            uint8_t * bitmap_out = (uint8_t *)draw_buf->data;
            bool byte_aligned = false;  // We use non-aligned format like static fonts
            
            // Use the generic 1bpp to A8 conversion function
            lv_font_convert_bitmap_1bpp_to_a8(bitmap_in, bitmap_out, g_dsc->box_w, g_dsc->box_h, byte_aligned);
            
            return draw_buf;
        } else {
            return NULL;
        }
    }
    
    return NULL;
}

/*-----------------
 *  PUBLIC FONT
 *----------------*/

const lv_font_t lv_font_k10_16 = {
    .get_glyph_dsc = k10_get_glyph_dsc,      // Use dynamic callback function
    .get_glyph_bitmap = k10_get_glyph_bitmap, // Use dynamic callback function
    .line_height = K10_ASCII_HEIGHT + 2,     // Use ASCII character height for line spacing
    .base_line = 2,
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = NULL,  // No static descriptor needed for dynamic fonts
    .fallback = NULL
};

#endif /*LV_FONT_K10_16*/
