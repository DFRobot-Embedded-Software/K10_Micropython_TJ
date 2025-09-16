/*******************************************************************************
 * K10 Font Chip Driver - GT30L24A3W Implementation
 * Real SPI communication implementation for MicroPython
 ******************************************************************************/

#include <string.h>
#include <stdint.h>
#include "py/runtime.h"
#include "py/mphal.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

/*-----------------
 *  FONT CHIP CONFIG
 *----------------*/

#define FONTCS                 40
#define ASCII_8X16             5

// SPI configuration
#define SPI_HOST               SPI3_HOST  // Arduino SPI1 corresponds to IDF SPI3
#define SPI_FREQUENCY          1000000    // 1MHz for testing
#define SPI_MODE               0          // SPI mode 0

// Global variables
static spi_device_handle_t spi_device = NULL;
static SemaphoreHandle_t spi_mutex = NULL;
static bool spi_initialized = false;

/*-----------------
 *  SPI FUNCTIONS
 *----------------*/

// Initialize SPI communication
static int k10_spi_init(void) {
    if (spi_initialized) {
        mp_printf(&mp_plat_print, "K10_SPI: Already initialized\n");
        mp_hal_delay_ms(10);
        return 1;
    }
    
    mp_printf(&mp_plat_print, "K10_SPI: Starting SPI initialization\n");
    mp_hal_delay_ms(10);
    
    // Create SPI mutex
    if (spi_mutex == NULL) {
        spi_mutex = xSemaphoreCreateMutex();
        if (spi_mutex == NULL) {
            mp_printf(&mp_plat_print, "K10_SPI: Failed to create SPI mutex\n");
            mp_hal_delay_ms(10);
            return 0;
        }
        mp_printf(&mp_plat_print, "K10_SPI: SPI mutex created\n");
        mp_hal_delay_ms(10);
    }
    
    // Configure GPIO for CS pin
    mp_printf(&mp_plat_print, "K10_SPI: Configuring CS pin (GPIO %d)\n", FONTCS);
    mp_hal_delay_ms(10);
    
    gpio_config_t cs_config = {
        .pin_bit_mask = (1ULL << FONTCS),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cs_config);
    gpio_set_level(FONTCS, 0);  // CS low (inactive) - Arduino style
    
    mp_printf(&mp_plat_print, "K10_SPI: CS pin configured, set to low (Arduino style)\n");
    mp_hal_delay_ms(10);
    
    // Test GPIO functionality
    mp_printf(&mp_plat_print, "K10_SPI: Testing GPIO functionality...\n");
    mp_hal_delay_ms(10);
    
    gpio_set_level(FONTCS, 1);
    int cs_level = gpio_get_level(FONTCS);
    mp_printf(&mp_plat_print, "K10_SPI: CS set to 1, read back: %d\n", cs_level);
    mp_hal_delay_ms(10);
    
    gpio_set_level(FONTCS, 0);
    cs_level = gpio_get_level(FONTCS);
    mp_printf(&mp_plat_print, "K10_SPI: CS set to 0, read back: %d\n", cs_level);
    mp_hal_delay_ms(10);
    
    // Try to initialize SPI bus, but don't fail if it's already initialized
    mp_printf(&mp_plat_print, "K10_SPI: Attempting to initialize SPI bus (MOSI:%d, MISO:%d, SCLK:%d)\n", 42, 41, 44);
    mp_hal_delay_ms(10);
    
    spi_bus_config_t bus_config = {
        .mosi_io_num = 42,  // MOSI pin
        .miso_io_num = 41,  // MISO pin
        .sclk_io_num = 44,  // SCLK pin
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    
    esp_err_t ret = spi_bus_initialize(SPI_HOST, &bus_config, SPI_DMA_CH_AUTO);
    if (ret == ESP_OK) {
        mp_printf(&mp_plat_print, "K10_SPI: SPI bus initialized successfully\n");
        mp_hal_delay_ms(10);
    } else if (ret == ESP_ERR_INVALID_STATE) {
        mp_printf(&mp_plat_print, "K10_SPI: SPI bus already initialized (shared with SD card)\n");
        mp_hal_delay_ms(10);
    } else {
        mp_printf(&mp_plat_print, "K10_SPI: Failed to initialize SPI bus: %s\n", esp_err_to_name(ret));
        mp_hal_delay_ms(10);
        return 0;
    }
    
    // Configure SPI device
    mp_printf(&mp_plat_print, "K10_SPI: Adding SPI device (freq:%d Hz, mode:%d)\n", SPI_FREQUENCY, SPI_MODE);
    mp_hal_delay_ms(10);
    
    spi_device_interface_config_t dev_config = {
        .clock_speed_hz = SPI_FREQUENCY,
        .mode = SPI_MODE,
        .spics_io_num = -1,  // We'll handle CS manually
        .queue_size = 1,
        .flags = 0,
    };
    
    ret = spi_bus_add_device(SPI_HOST, &dev_config, &spi_device);
    if (ret != ESP_OK) {
        mp_printf(&mp_plat_print, "K10_SPI: Failed to add SPI device: %s\n", esp_err_to_name(ret));
        mp_hal_delay_ms(10);
        return 0;
    }
    
    mp_printf(&mp_plat_print, "K10_SPI: SPI device added successfully, handle: %p\n", spi_device);
    mp_hal_delay_ms(10);
    
    // Test SPI communication with a simple transaction
    mp_printf(&mp_plat_print, "K10_SPI: Testing SPI communication...\n");
    mp_hal_delay_ms(10);
    
    // Test transaction
    unsigned char test_tx[1] = {0x00};
    unsigned char test_rx[1] = {0x00};
    
    spi_transaction_t test_trans = {
        .length = 8,  // 1 byte in bits
        .tx_buffer = test_tx,
        .rx_buffer = test_rx,
    };
    
    esp_err_t test_ret = spi_device_transmit(spi_device, &test_trans);
    if (test_ret == ESP_OK) {
        mp_printf(&mp_plat_print, "K10_SPI: Test transaction successful, received: 0x%02X\n", test_rx[0]);
    } else {
        mp_printf(&mp_plat_print, "K10_SPI: Test transaction failed: %s\n", esp_err_to_name(test_ret));
    }
    mp_hal_delay_ms(10);
    
    spi_initialized = true;
    mp_printf(&mp_plat_print, "K10_SPI: SPI initialization completed successfully\n");
    mp_hal_delay_ms(10);
    return 1;
}

// SPI read data function
static unsigned char k10_spi_read_data(unsigned char* sendbuf, unsigned char sendlen, 
                                      unsigned char* receivebuf, unsigned int receivelen) {
    if (!spi_initialized || spi_device == NULL) {
        mp_printf(&mp_plat_print, "K10_SPI: SPI not initialized or device is NULL\n");
        mp_hal_delay_ms(10);
        return 0;
    }
    
    mp_printf(&mp_plat_print, "K10_SPI: Starting SPI transaction (send:%d bytes, recv:%d bytes)\n", sendlen, receivelen);
    mp_hal_delay_ms(10);
    
    xSemaphoreTake(spi_mutex, portMAX_DELAY);
    mp_printf(&mp_plat_print, "K10_SPI: SPI mutex acquired\n");
    mp_hal_delay_ms(10);
    
    // CS high (active) - Arduino style
    gpio_set_level(FONTCS, 1);
    mp_printf(&mp_plat_print, "K10_SPI: CS set to high (active, Arduino style)\n");
    mp_hal_delay_ms(10);
    
    // Send command and address
    if (sendlen > 0) {
        mp_printf(&mp_plat_print, "K10_SPI: Sending %d bytes: ", sendlen);
        for (int i = 0; i < sendlen; i++) {
            mp_printf(&mp_plat_print, "0x%02X ", sendbuf[i]);
        }
        mp_printf(&mp_plat_print, "\n");
        mp_hal_delay_ms(10);
        
        spi_transaction_t trans = {
            .length = sendlen * 8,  // Length in bits
            .tx_buffer = sendbuf,
            .rx_buffer = NULL,
        };
        esp_err_t ret = spi_device_transmit(spi_device, &trans);
        if (ret != ESP_OK) {
            mp_printf(&mp_plat_print, "K10_SPI: Send transaction failed: %s\n", esp_err_to_name(ret));
            mp_hal_delay_ms(10);
            gpio_set_level(FONTCS, 1);  // CS high (inactive)
            xSemaphoreGive(spi_mutex);
            return 0;
        }
        mp_printf(&mp_plat_print, "K10_SPI: Send transaction completed\n");
        mp_hal_delay_ms(10);
    }
    
    // Receive data
    if (receivelen > 0) {
        mp_printf(&mp_plat_print, "K10_SPI: Receiving %d bytes\n", receivelen);
        mp_hal_delay_ms(10);
        
        spi_transaction_t trans = {
            .length = receivelen * 8,  // Length in bits
            .tx_buffer = NULL,
            .rx_buffer = receivebuf,
        };
        esp_err_t ret = spi_device_transmit(spi_device, &trans);
        if (ret != ESP_OK) {
            mp_printf(&mp_plat_print, "K10_SPI: Receive transaction failed: %s\n", esp_err_to_name(ret));
            mp_hal_delay_ms(10);
            gpio_set_level(FONTCS, 1);  // CS high (inactive)
            xSemaphoreGive(spi_mutex);
            return 0;
        }
        
        mp_printf(&mp_plat_print, "K10_SPI: Received data: ");
        for (int i = 0; i < receivelen && i < 16; i++) {  // Print first 16 bytes
            mp_printf(&mp_plat_print, "0x%02X ", receivebuf[i]);
        }
        if (receivelen > 16) {
            mp_printf(&mp_plat_print, "...");
        }
        mp_printf(&mp_plat_print, "\n");
        mp_hal_delay_ms(10);
    }
    
    // CS low (inactive) - Arduino style
    gpio_set_level(FONTCS, 0);
    mp_printf(&mp_plat_print, "K10_SPI: CS set to low (inactive, Arduino style)\n");
    mp_hal_delay_ms(10);
    
    xSemaphoreGive(spi_mutex);
    mp_printf(&mp_plat_print, "K10_SPI: SPI mutex released\n");
    mp_hal_delay_ms(10);
    
    mp_printf(&mp_plat_print, "K10_SPI: SPI transaction completed successfully\n");
    mp_hal_delay_ms(10);
    return 1;
}

// Read data batch from font chip
static unsigned long k10_read_data_batch(unsigned long address, unsigned long DataLen, unsigned char *pBuff) {
    if (!spi_initialized || spi_device == NULL) {
        mp_printf(&mp_plat_print, "K10_SPI: Batch read failed - SPI not initialized\n");
        mp_hal_delay_ms(10);
        return 0;
    }
    
    mp_printf(&mp_plat_print, "K10_SPI: Batch read - address:0x%06lX, length:%lu bytes\n", address, DataLen);
    mp_hal_delay_ms(10);
    
    xSemaphoreTake(spi_mutex, portMAX_DELAY);
    mp_printf(&mp_plat_print, "K10_SPI: Batch read mutex acquired\n");
    mp_hal_delay_ms(10);
    
    // CS high (active) - Arduino style
    gpio_set_level(FONTCS, 1);
    mp_printf(&mp_plat_print, "K10_SPI: Batch read CS set to high (Arduino style)\n");
    mp_hal_delay_ms(10);
    
    // Send read command (0x03) and 24-bit address
    unsigned char cmd[4];
    cmd[0] = 0x03;  // Read command
    cmd[1] = (unsigned char)((address) >> 16);
    cmd[2] = (unsigned char)((address) >> 8);
    cmd[3] = (unsigned char)address;
    
    mp_printf(&mp_plat_print, "K10_SPI: Batch read command: 0x%02X 0x%02X 0x%02X 0x%02X\n", 
              cmd[0], cmd[1], cmd[2], cmd[3]);
    mp_hal_delay_ms(10);
    
    spi_transaction_t trans = {
        .length = 4 * 8,  // 4 bytes in bits
        .tx_buffer = cmd,
        .rx_buffer = NULL,
    };
    
    mp_printf(&mp_plat_print, "K10_SPI: Sending command transaction (length:%d bits)\n", trans.length);
    mp_hal_delay_ms(10);
    
    esp_err_t ret = spi_device_transmit(spi_device, &trans);
    if (ret != ESP_OK) {
        mp_printf(&mp_plat_print, "K10_SPI: Batch read command failed: %s\n", esp_err_to_name(ret));
        mp_hal_delay_ms(10);
        gpio_set_level(FONTCS, 1);  // CS high (inactive)
        xSemaphoreGive(spi_mutex);
        return 0;
    }
    
    mp_printf(&mp_plat_print, "K10_SPI: Command transaction completed, checking CS state\n");
    mp_hal_delay_ms(10);
    
    // Check CS pin state
    int cs_level = gpio_get_level(FONTCS);
    mp_printf(&mp_plat_print, "K10_SPI: CS pin level after command: %d\n", cs_level);
    mp_hal_delay_ms(10);
    
    mp_printf(&mp_plat_print, "K10_SPI: Batch read command sent successfully\n");
    mp_hal_delay_ms(10);
    
    // Read data
    if (DataLen > 0) {
        mp_printf(&mp_plat_print, "K10_SPI: Batch read receiving %lu bytes\n", DataLen);
        mp_hal_delay_ms(10);
        
        // Clear buffer before reading
        memset(pBuff, 0x00, DataLen);
        mp_printf(&mp_plat_print, "K10_SPI: Buffer cleared before read\n");
        mp_hal_delay_ms(10);
        
        spi_transaction_t trans = {
            .length = DataLen * 8,  // Length in bits
            .tx_buffer = NULL,
            .rx_buffer = pBuff,
        };
        
        mp_printf(&mp_plat_print, "K10_SPI: Sending data read transaction (length:%lu bits)\n", trans.length);
        mp_hal_delay_ms(10);
        
        ret = spi_device_transmit(spi_device, &trans);
        if (ret != ESP_OK) {
            mp_printf(&mp_plat_print, "K10_SPI: Batch read data failed: %s\n", esp_err_to_name(ret));
            mp_hal_delay_ms(10);
            gpio_set_level(FONTCS, 1);  // CS high (inactive)
            xSemaphoreGive(spi_mutex);
            return 0;
        }
        
        mp_printf(&mp_plat_print, "K10_SPI: Data read transaction completed\n");
        mp_hal_delay_ms(10);
        
        mp_printf(&mp_plat_print, "K10_SPI: Batch read data received: ");
        for (int i = 0; i < DataLen && i < 16; i++) {  // Print first 16 bytes
            mp_printf(&mp_plat_print, "0x%02X ", pBuff[i]);
        }
        if (DataLen > 16) {
            mp_printf(&mp_plat_print, "...");
        }
        mp_printf(&mp_plat_print, "\n");
        mp_hal_delay_ms(10);
    }
    
    // CS low (inactive) - Arduino style
    gpio_set_level(FONTCS, 0);
    mp_printf(&mp_plat_print, "K10_SPI: Batch read CS set to low (Arduino style)\n");
    mp_hal_delay_ms(10);
    
    xSemaphoreGive(spi_mutex);
    mp_printf(&mp_plat_print, "K10_SPI: Batch read mutex released\n");
    mp_hal_delay_ms(10);
    
    mp_printf(&mp_plat_print, "K10_SPI: Batch read completed, first byte: 0x%02X\n", pBuff[0]);
    mp_hal_delay_ms(10);
    return pBuff[0];
}

/*-----------------
 *  FONT CHIP FUNCTIONS
 *----------------*/

// Initialize font chip
int GT_Font_Init(void) {
    mp_printf(&mp_plat_print, "K10_FONT: GT_Font_Init called\n");
    mp_hal_delay_ms(10);
    
    int result = k10_spi_init();
    
    if (result) {
        mp_printf(&mp_plat_print, "K10_FONT: GT_Font_Init completed successfully\n");
    } else {
        mp_printf(&mp_plat_print, "K10_FONT: GT_Font_Init failed\n");
    }
    mp_hal_delay_ms(10);
    
    return result;
}

// Get ASCII character data from font chip
unsigned char ASCII_GetData(unsigned char asc, unsigned long ascii_kind, unsigned char *DZ_Data) {
    mp_printf(&mp_plat_print, "K10_FONT: ASCII_GetData called - char:%d (0x%02X), format:%lu\n", asc, asc, ascii_kind);
    mp_hal_delay_ms(10);
    
    // Check if this is the format we support
    if (ascii_kind != ASCII_8X16) {
        mp_printf(&mp_plat_print, "K10_FONT: Unsupported format: %lu (expected: %d)\n", ascii_kind, ASCII_8X16);
        mp_hal_delay_ms(10);
        return 0;  // Unsupported format
    }
    
    // Check character range (printable ASCII)
    if (asc < 32 || asc > 126) {
        mp_printf(&mp_plat_print, "K10_FONT: Unsupported character: %d (0x%02X)\n", asc, asc);
        mp_hal_delay_ms(10);
        return 0;  // Unsupported character
    }
    
    mp_printf(&mp_plat_print, "K10_FONT: Valid ASCII character: %d (0x%02X)\n", asc, asc);
    mp_hal_delay_ms(10);
    
    // Initialize SPI if not already done
    if (!k10_spi_init()) {
        mp_printf(&mp_plat_print, "K10_FONT: SPI initialization failed\n");
        mp_hal_delay_ms(10);
        return 0;
    }
    
    // Calculate address for ASCII_8X16 format
    // Based on the Arduino implementation: r_dat_bat(16 * (asc + 130174), 0x10u, DZ_Data);
    unsigned long address = 16 * (asc + 130174);
    mp_printf(&mp_plat_print, "K10_FONT: Calculated address: 0x%06lX (16 * (%d + 130174))\n", address, asc);
    mp_hal_delay_ms(10);
    
    // Read 16 bytes of data (8x16 pixels = 16 bytes for 1bpp)
    unsigned long result = k10_read_data_batch(address, 16, DZ_Data);
    mp_printf(&mp_plat_print, "K10_FONT: k10_read_data_batch returned: 0x%02lX\n", result);
    mp_hal_delay_ms(10);
    
    // Check if we got valid data (not all 0xFF or all 0x00)
    bool has_valid_data = false;
    for (int i = 0; i < 16; i++) {
        if (DZ_Data[i] != 0xFF && DZ_Data[i] != 0x00) {
            has_valid_data = true;
            break;
        }
    }
    
    if (has_valid_data) {
        mp_printf(&mp_plat_print, "K10_FONT: ASCII_GetData completed successfully (valid data detected)\n");
        mp_hal_delay_ms(10);
        return 1;  // Success
    }
    
    mp_printf(&mp_plat_print, "K10_FONT: ASCII_GetData failed (no valid data)\n");
    mp_hal_delay_ms(10);
    return 0;  // Failed
}

// Get character interval/spacing (optional)
unsigned char ASCII_GetInterval(unsigned char asc, unsigned long ascii_kind) {
    if (ascii_kind != ASCII_8X16) {
        return 0;
    }
    
    if (asc < 32 || asc > 126) {
        return 0;
    }
    
    // Initialize SPI if not already done
    if (!k10_spi_init()) {
        return 0;
    }
    
    // Calculate address for interval data
    unsigned long address = 16 * (asc + 130174);
    unsigned char data[2];
    
    // Read 2 bytes of interval data
    if (k10_read_data_batch(address, 2, data)) {
        return data[1];  // Return the interval value
    }
    
    return 1;  // Default spacing
}

// Get GBK character data (for Chinese characters)
unsigned long GBK_24_GetData(unsigned char c1, unsigned char c2, unsigned char *DZ_Data) {
    // Initialize SPI if not already done
    if (!k10_spi_init()) {
        return 0;
    }
    
    // Calculate address based on GBK encoding
    // This is a simplified version - you may need to adjust based on actual font chip layout
    unsigned long address = 0;
    
    // Basic GBK address calculation (simplified)
    if (c1 >= 0xA1 && c1 <= 0xFE && c2 >= 0xA1 && c2 <= 0xFE) {
        address = 72 * ((c1 - 0xA1) * 94 + (c2 - 0xA1));
    } else {
        return 0;  // Invalid GBK range
    }
    
    // Read 72 bytes of data (12x24 pixels = 72 bytes for 1bpp)
    if (k10_read_data_batch(address, 72, DZ_Data)) {
        return address;  // Return the address used
    }
    
    return 0;  // Failed
}

// Unicode to GBK conversion (simplified)
unsigned long U2G(unsigned int unicode) {
    // This is a simplified conversion - you may need a complete Unicode to GBK table
    // For now, return a basic conversion for common Chinese characters
    
    if (unicode >= 0x4E00 && unicode <= 0x9FFF) {
        // CJK Unified Ideographs range
        // Basic conversion (this is not complete)
        unsigned char c1 = 0xA1 + ((unicode - 0x4E00) / 94);
        unsigned char c2 = 0xA1 + ((unicode - 0x4E00) % 94);
        return (c1 << 8) | c2;
    }
    
    return 0;  // No conversion available
}

// Get 12x12 character data
void gt_12_GetData(unsigned char MSB, unsigned char LSB, unsigned char *DZ_Data) {
    // Initialize SPI if not already done
    if (!k10_spi_init()) {
        memset(DZ_Data, 0, 18);  // 12x12 pixels = 18 bytes
        return;
    }
    
    // Calculate address for 12x12 characters
    // This is based on the Arduino implementation
    unsigned long address = 18 * (94 * MSB + LSB) + 2109216;
    
    // Read 18 bytes of data (12x12 pixels = 18 bytes for 1bpp)
    k10_read_data_batch(address, 18, DZ_Data);
}

// Read data batch from font chip (public interface)
unsigned long r_dat_bat(unsigned long address, unsigned long DataLen, unsigned char *pBuff) {
    return k10_read_data_batch(address, DataLen, pBuff);
}

// SPI communication function (public interface)
unsigned char gt_read_data(unsigned char* sendbuf, unsigned char sendlen, 
                          unsigned char* receivebuf, unsigned int receivelen) {
    return k10_spi_read_data(sendbuf, sendlen, receivebuf, receivelen);
}