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

// Map ASCII range 32-126
static const lv_font_fmt_txt_cmap_t k10_ascii_cmap = {
    .range_start = K10_ASCII_START,
    .range_length = K10_ASCII_COUNT,
    .glyph_id_start = 1,  // ASCII characters start at glyph_id 1
    .unicode_list = NULL,
    .glyph_id_ofs_list = NULL,
    .list_length = 0,
    .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
};

// Map Chinese characters (GBK range)
static const lv_font_fmt_txt_cmap_t k10_chinese_cmap = {
    .range_start = K10_GBK_START,
    .range_length = K10_GBK_COUNT,
    .glyph_id_start = K10_ASCII_COUNT + 1,  // Chinese characters start after ASCII
    .unicode_list = NULL,
    .glyph_id_ofs_list = NULL,
    .list_length = 0,
    .type = LV_FONT_FMT_TXT_CMAP_FORMAT0_TINY
};

// Combined character maps
static const lv_font_fmt_txt_cmap_t k10_cmaps[] = {
    k10_ascii_cmap,
    k10_chinese_cmap
};

/*-----------------
 *  CHARACTER CACHE SYSTEM
 *----------------*/

// Cache for recently used characters
#define K10_CACHE_SIZE 32

typedef struct {
    uint32_t unicode;
    uint8_t bitmap_data[K10_CHINESE_BYTES];  // Use largest size
    bool is_ascii;
    bool valid;
} k10_char_cache_t;

static k10_char_cache_t k10_cache[K10_CACHE_SIZE];
static int k10_cache_index = 0;

// Initialize cache
static void k10_init_cache(void) {
    for (int i = 0; i < K10_CACHE_SIZE; i++) {
        k10_cache[i].valid = false;
    }
}

// Get character from cache or load from font chip
static const uint8_t* k10_get_char_data(uint32_t unicode) {
    // Check cache first
    for (int i = 0; i < K10_CACHE_SIZE; i++) {
        if (k10_cache[i].valid && k10_cache[i].unicode == unicode) {
            mp_printf(&mp_plat_print, " (cached)");
            return k10_cache[i].bitmap_data;
        }
    }
    
    mp_printf(&mp_plat_print, " (loading)");
    
    // Not in cache, load from font chip
    if (!k10_font_init()) {
        mp_printf(&mp_plat_print, " init failed");
        return NULL;
    }
    
    // Find empty cache slot
    int slot = k10_cache_index;
    k10_cache_index = (k10_cache_index + 1) % K10_CACHE_SIZE;
    
    k10_char_cache_t *entry = &k10_cache[slot];
    entry->unicode = unicode;
    entry->valid = true;
    
    // Determine if it's ASCII or Chinese
    if (unicode >= K10_ASCII_START && unicode <= K10_ASCII_END) {
        // ASCII character
        entry->is_ascii = true;
        mp_printf(&mp_plat_print, " ASCII");
        unsigned char result = ASCII_GetData((unsigned char)unicode, ASCII_8X16, entry->bitmap_data);
        if (result == 0) {
            memset(entry->bitmap_data, 0xFF, K10_ASCII_BYTES);
        }
    } else if (unicode >= K10_GBK_START && unicode <= K10_GBK_END) {
        // Chinese character
        entry->is_ascii = false;
        unsigned char c1 = (unicode >> 8) & 0xFF;
        unsigned char c2 = unicode & 0xFF;
        mp_printf(&mp_plat_print, " Chinese GBK:0x%02X%02X", c1, c2);
        unsigned long result = GBK_24_GetData(c1, c2, entry->bitmap_data);
        if (result == 0) {
            memset(entry->bitmap_data, 0xFF, K10_CHINESE_BYTES);
        }
    } else {
        // Unsupported character
        mp_printf(&mp_plat_print, " unsupported");
        entry->valid = false;
        return NULL;
    }
    
    return entry->bitmap_data;
}

/*-----------------
 *  CUSTOM FONT FUNCTIONS
 *----------------*/

// Custom glyph descriptor function
static bool k10_get_glyph_dsc(const lv_font_t * font, lv_font_glyph_dsc_t * dsc_out, uint32_t unicode_letter, uint32_t unicode_letter_next) {
    (void)font;
    (void)unicode_letter_next;
    
    // Debug print
    mp_printf(&mp_plat_print, "[K10_FONT] get_glyph_dsc: 0x%04X", (unsigned int)unicode_letter);
    
    // Check if character is supported
    if ((unicode_letter >= K10_ASCII_START && unicode_letter <= K10_ASCII_END) ||
        (unicode_letter >= K10_GBK_START && unicode_letter <= K10_GBK_END)) {
        
        // Determine character dimensions
        uint16_t width, height;
        if (unicode_letter >= K10_ASCII_START && unicode_letter <= K10_ASCII_END) {
            width = K10_ASCII_WIDTH;
            height = K10_ASCII_HEIGHT;
            mp_printf(&mp_plat_print, " (ASCII %dx%d)", width, height);
        } else {
            width = K10_CHINESE_WIDTH;
            height = K10_CHINESE_HEIGHT;
            mp_printf(&mp_plat_print, " (Chinese %dx%d)", width, height);
        }
        
        // Set glyph descriptor
        *dsc_out = (lv_font_glyph_dsc_t){
            .resolved_font = font,
            .adv_w = width * 16,  // 8.4 format
            .box_w = width,
            .box_h = height,
            .ofs_x = 0,
            .ofs_y = 0,
            .format = LV_FONT_GLYPH_FORMAT_A1,  // 1bpp format
            .is_placeholder = 0,
            .req_raw_bitmap = 1,  // Request raw bitmap format
            .gid = {.index = unicode_letter},
            .entry = NULL
        };
        
        mp_printf(&mp_plat_print, " -> OK\n");
        return true;
    }
    
    mp_printf(&mp_plat_print, " -> NOT SUPPORTED\n");
    return false;
}

// Custom bitmap function
static const void * k10_get_glyph_bitmap(lv_font_glyph_dsc_t * g_dsc, lv_draw_buf_t * draw_buf) {
    (void)draw_buf;  // We don't use the draw buffer
    
    // Get the unicode character from the glyph descriptor
    uint32_t unicode_letter = g_dsc->gid.index;
    
    mp_printf(&mp_plat_print, "[K10_FONT] get_glyph_bitmap: 0x%04X", (unsigned int)unicode_letter);
    
    // Get character data from cache or font chip
    const uint8_t* bitmap_data = k10_get_char_data(unicode_letter);
    
    if (bitmap_data == NULL) {
        mp_printf(&mp_plat_print, " -> NULL, using dummy\n");
        // Return dummy data for unsupported characters
        static uint8_t dummy_data[K10_CHINESE_BYTES];
        memset(dummy_data, 0xFF, sizeof(dummy_data));
        return dummy_data;
    }
    
    mp_printf(&mp_plat_print, " -> OK\n");
    return bitmap_data;
}

/*-----------------
 *  FONT DESCRIPTOR
 *----------------*/

static const lv_font_fmt_txt_dsc_t k10_font_dsc = {
    .glyph_bitmap = NULL,  // We use dynamic loading
    .glyph_dsc = NULL,     // We use custom functions
    .cmaps = k10_cmaps,
    .kern_dsc = NULL,
    .kern_scale = 0,
    .cmap_num = 2,  // ASCII + Chinese
    .bpp = K10_FONT_BPP,
    .kern_classes = 0,
    .bitmap_format = 0
};

/*-----------------
 *  FONT INITIALIZATION
 *----------------*/

// Initialize the font system
static void k10_font_system_init(void) {
    static bool initialized = false;
    if (!initialized) {
        k10_init_cache();
        initialized = true;
    }
}

/*-----------------
 *  PUBLIC FONT
 *----------------*/

const lv_font_t lv_font_k10_16 = {
    .get_glyph_dsc = k10_get_glyph_dsc,    // Use our custom function
    .get_glyph_bitmap = k10_get_glyph_bitmap,    // Use our custom function
    .line_height = K10_CHINESE_HEIGHT + 2,  // Use Chinese character height for line spacing
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

// Constructor function to initialize the font
__attribute__((constructor))
static void k10_font_constructor(void) {
    k10_font_system_init();
}

#endif /*LV_FONT_K10_16*/
