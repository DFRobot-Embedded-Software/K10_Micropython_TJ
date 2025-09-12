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
 *  CHARACTER MAPPING
 *----------------*/

// Map only character 'A' (ASCII 65)
static const lv_font_fmt_txt_cmap_t k10_ascii_cmap = {
    .range_start = 65,  // 'A'
    .range_length = 1,  // Only one character
    .glyph_id_start = 1,  // Character 'A' starts at glyph_id 1
    .unicode_list = NULL,
    .glyph_id_ofs_list = NULL,
    .list_length = 0,
    .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
};

// Combined character maps
static const lv_font_fmt_txt_cmap_t k10_cmaps[] = {
    k10_ascii_cmap
};

/*-----------------
 *  FONT INITIALIZATION
 *----------------*/

// Initialize the font system
static void k10_font_system_init(void) {
    static bool initialized = false;
    if (!initialized) {
        // Initialize font chip
        GT_Font_Init();
        initialized = true;
    }
}

/*-----------------
 *  GLYPH DESCRIPTION
 *----------------*/

// Simple glyph descriptor for character 'A' (ASCII 65)
static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 128, .box_w = 8, .box_h = 16, .ofs_x = 0, .ofs_y = 0} /* 'A' */,
};

/*-----------------
 *  CHARACTER BITMAP
 *----------------*/

// Simple bitmap data for character 'A' (8x16 pixels, 1bpp)
static const uint8_t glyph_bitmap[] = {
    /* 'A' */
    0x18, 0x24, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/*-----------------
 *  FONT DESCRIPTOR
 *----------------*/

static const lv_font_fmt_txt_dsc_t k10_font_dsc = {
    .glyph_bitmap = glyph_bitmap,
    .glyph_dsc = glyph_dsc,
    .cmaps = k10_cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,         // Only ASCII
    .bpp = K10_FONT_BPP,
    .kern_classes = 0,
    .bitmap_format = 0
};


/*-----------------
 *  PUBLIC FONT
 *----------------*/

const lv_font_t lv_font_k10_16 = {
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    // Use LVGL standard function
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    // Use LVGL standard function
    .line_height = K10_ASCII_HEIGHT + 2,  // Use ASCII character height for line spacing
    .base_line = 2,
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &k10_font_dsc
};


#endif /*LV_FONT_K10_16*/
