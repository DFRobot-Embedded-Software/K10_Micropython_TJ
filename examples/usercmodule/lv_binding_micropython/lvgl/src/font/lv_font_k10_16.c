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
#ifndef LV_FONT_K10_24
    #define LV_FONT_K10_24 1
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

// Font dimensions for Chinese characters (24x24)
#define K10_CHINESE_WIDTH      24
#define K10_CHINESE_HEIGHT     24
#define K10_CHINESE_BYTES      ((K10_CHINESE_WIDTH * K10_CHINESE_HEIGHT) / 8)  // 72 bytes

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

// Buffer for dynamic font data (1bpp from chip) - use larger size for Chinese characters
static uint8_t k10_font_buffer[K10_CHINESE_BYTES];  // 72 bytes for 24x24 Chinese characters

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
    
    // Check if it's a printable ASCII character or Chinese character
    bool is_ascii = (unicode_letter >= 32 && unicode_letter <= 126);
    bool is_chinese = (unicode_letter >= 0x4E00 && unicode_letter <= 0x9FFF);  // CJK Unified Ideographs
    
    if (!is_ascii && !is_chinese) {
        return false;  // Character not supported
    }
    
    // Set glyph descriptor based on character type
    dsc_out->resolved_font = font;       // Set the resolved font
    dsc_out->box_h = 12;   /* Height of the glyph bitmap (in pixels) */
    dsc_out->box_w = 16;   /* Width of the glyph bitmap (in pixels) */
    if(unicode_letter < 128){
        dsc_out->adv_w = ASCII_GetInterval(unicode_letter,ASCII_12_A);   /* Letter spacing */
      }else{
        dsc_out->adv_w = 12;   
      }
    dsc_out->ofs_x = 0;                  // X offset of the glyph bitmap (in pixels)
    dsc_out->ofs_y = 0; // Shift Chinese glyphs for top alignment (negative moves bitmap down relative to baseline)
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
    bool is_ascii = (unicode_letter >= 32 && unicode_letter <= 126);
    bool is_chinese = (unicode_letter >= 0x4E00 && unicode_letter <= 0x9FFF);  // CJK Unified Ideographs
    
    if (!is_ascii && !is_chinese) {
        return NULL;
    }
    
    // Initialize font chip if needed
    if (!k10_font_init()) {
        return NULL;
    }
    
    // Get font data from K10 chip based on character type
    bool data_retrieved = false;
    
    if (is_ascii) {
        // Get ASCII character data
        data_retrieved = ASCII_GetData((unsigned char)unicode_letter, ASCII_12_A, k10_font_buffer);
    } else if (is_chinese) {
        // Convert Unicode to GBK and get Chinese character data
        unsigned long gbk_code = U2G(unicode_letter);
        if (gbk_code != 0) {
            unsigned char c1 = (gbk_code >> 8) & 0xFF;
            unsigned char c2 = gbk_code & 0xFF;
            gt_12_GetData(c1, c2, k10_font_buffer);
        }
    }
    
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

    
    return NULL;
}

static bool k10_get_glyph_dsc_24(const lv_font_t * font, lv_font_glyph_dsc_t * dsc_out, uint32_t unicode_letter, uint32_t unicode_letter_next) {
    // Safety check
    if (!font || !dsc_out) {
        return false;
    }
    
    // Initialize font chip if needed
    if (!k10_font_init()) {
        return false;
    }
    
    // Check if it's a printable ASCII character or Chinese character
    bool is_ascii = (unicode_letter >= 32 && unicode_letter <= 126);
    bool is_chinese = (unicode_letter >= 0x4E00 && unicode_letter <= 0x9FFF);  // CJK Unified Ideographs
    
    if (!is_ascii && !is_chinese) {
        return false;  // Character not supported
    }
    
    // Set glyph descriptor based on character type
    dsc_out->resolved_font = font;       // Set the resolved font
    dsc_out->box_h = 24;   /* Height of the glyph bitmap (in pixels) */
    dsc_out->box_w = 24;   /* Width of the glyph bitmap (in pixels) */
    if(unicode_letter < 128){
        dsc_out->adv_w = ASCII_GetInterval(unicode_letter,ASCII_24_B);   /* Letter spacing */
      }else{
        dsc_out->adv_w = 24;   
      }
    dsc_out->ofs_x = 0;                  // X offset of the glyph bitmap (in pixels)
    dsc_out->ofs_y = 0; // Shift Chinese glyphs for top alignment (negative moves bitmap down relative to baseline)
    dsc_out->format = LV_FONT_GLYPH_FORMAT_A1;  // Original format is 1bpp
    dsc_out->is_placeholder = false;
    dsc_out->req_raw_bitmap = 0;         // We'll do the conversion ourselves
    dsc_out->gid.index = unicode_letter; // Store the unicode character as glyph ID
    dsc_out->entry = NULL;               // No cache entry for dynamic fonts
    
    return true;  // Character found
}

// Dynamic glyph bitmap callback
static const void * k10_get_glyph_bitmap_24(lv_font_glyph_dsc_t * g_dsc, lv_draw_buf_t * draw_buf) {
    // Safety check
    if (!g_dsc) {
        return NULL;
    }
    
    uint32_t unicode_letter = g_dsc->gid.index;
    
    // Check if unicode_letter is valid
    bool is_ascii = (unicode_letter >= 32 && unicode_letter <= 126);
    bool is_chinese = (unicode_letter >= 0x4E00 && unicode_letter <= 0x9FFF);  // CJK Unified Ideographs
    
    if (!is_ascii && !is_chinese) {
        return NULL;
    }
    
    // Initialize font chip if needed
    if (!k10_font_init()) {
        return NULL;
    }
    
    // Get font data from K10 chip based on character type
    bool data_retrieved = false;
    
    if (is_ascii) {
        // Get ASCII character data
        data_retrieved = ASCII_GetData((unsigned char)unicode_letter, ASCII_24_B, k10_font_buffer);
    } else if (is_chinese) {
        // Convert Unicode to GBK and get Chinese character data
        unsigned long gbk_code = U2G(unicode_letter);
        if (gbk_code != 0) {
            unsigned char c1 = (gbk_code >> 8) & 0xFF;
            unsigned char c2 = gbk_code & 0xFF;
            GBK_24_GetData(c1, c2, k10_font_buffer);
        }
    }
    
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

    
    return NULL;
}

/*-----------------
 *  PUBLIC FONT
 *----------------*/

const lv_font_t lv_font_k10_16 = {
    .get_glyph_dsc = k10_get_glyph_dsc,      // Use dynamic callback function
    .get_glyph_bitmap = k10_get_glyph_bitmap, // Use dynamic callback function
    .line_height = K10_ASCII_HEIGHT,       // Set line height to exact glyph height
    .base_line = 0,
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


const lv_font_t lv_font_k10_24 = {
    .get_glyph_dsc = k10_get_glyph_dsc_24,      // Use dynamic callback function
    .get_glyph_bitmap = k10_get_glyph_bitmap_24, // Use dynamic callback function
    .line_height = 24,       // Set line height to exact glyph height
    .base_line = 0,
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
