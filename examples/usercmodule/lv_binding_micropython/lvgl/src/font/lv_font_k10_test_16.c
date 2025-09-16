/*******************************************************************************
 * K10 Test Font - Static Font for Testing
 * Size: 16 px (ASCII_8X16 format)
 * Only contains character 'A' for testing purposes
 ******************************************************************************/

#ifdef LV_LVGL_H_INCLUDE_SIMPLE
    #include "lvgl.h"
#else
    #include "../../lvgl.h"
#endif

#ifndef LV_FONT_K10_TEST_16
    #define LV_FONT_K10_TEST_16 1
#endif

#if LV_FONT_K10_TEST_16

#include <string.h>
#include <stdint.h>

/*-----------------
 *  FONT DIMENSIONS
 *----------------*/

// Font dimensions for ASCII_8X16
#define K10_TEST_ASCII_WIDTH        8
#define K10_TEST_ASCII_HEIGHT       16
#define K10_TEST_ASCII_BYTES        ((K10_TEST_ASCII_WIDTH * K10_TEST_ASCII_HEIGHT) / 8)  // 16 bytes

#define K10_TEST_FONT_BPP           1

/*-----------------
 *  CHARACTER MAPPING
 *----------------*/

// Map only character 'A' (ASCII 65)
static const lv_font_fmt_txt_cmap_t k10_test_ascii_cmap = {
    .range_start = 65,  // 'A'
    .range_length = 1,  // Only one character
    .glyph_id_start = 1,  // Character 'A' starts at glyph_id 1
    .unicode_list = NULL,
    .glyph_id_ofs_list = NULL,
    .list_length = 0,
    .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
};

// Combined character maps
static const lv_font_fmt_txt_cmap_t k10_test_cmaps[] = {
    k10_test_ascii_cmap
};

/*-----------------
 *  GLYPH DESCRIPTION
 *----------------*/

// Simple glyph descriptor for character 'A' (ASCII 65)
static const lv_font_fmt_txt_glyph_dsc_t k10_test_glyph_dsc[] = {
    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0} /* id = 0 reserved */,
    {.bitmap_index = 0, .adv_w = 128, .box_w = 8, .box_h = 16, .ofs_x = 0, .ofs_y = 0} /* 'A' */,
};

/*-----------------
 *  CHARACTER BITMAP
 *----------------*/

// Static bitmap data for character 'A' (8x16 pixels, 1bpp)
static const uint8_t k10_test_glyph_bitmap[] = {
    /* 'A' */
    0x18, 0x24, 0x42, 0x7E, 0x42, 0x42, 0x42, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/*-----------------
 *  FONT DESCRIPTOR
 *----------------*/

static const lv_font_fmt_txt_dsc_t k10_test_font_dsc = {
    .glyph_bitmap = k10_test_glyph_bitmap,
    .glyph_dsc = k10_test_glyph_dsc,
    .cmaps = k10_test_cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 1,         // Only ASCII
    .bpp = K10_TEST_FONT_BPP,
    .kern_classes = 0,
    .bitmap_format = 0
};

/*-----------------
 *  PUBLIC FONT
 *----------------*/

const lv_font_t lv_font_k10_test_16 = {
    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,    // Use LVGL standard function
    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,    // Use LVGL standard function
    .line_height = K10_TEST_ASCII_HEIGHT + 2,  // Use ASCII character height for line spacing
    .base_line = 2,
#if !(LVGL_VERSION_MAJOR == 6 && LVGL_VERSION_MINOR == 0)
    .subpx = LV_FONT_SUBPX_NONE,
#endif
#if LV_VERSION_CHECK(7, 4, 0) || LVGL_VERSION_MAJOR >= 8
    .underline_position = -1,
    .underline_thickness = 1,
#endif
    .dsc = &k10_test_font_dsc
};

#endif /*LV_FONT_K10_TEST_16*/
