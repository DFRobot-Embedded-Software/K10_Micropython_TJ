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
        return 1;
    }
    
    // Create SPI mutex
    if (spi_mutex == NULL) {
        spi_mutex = xSemaphoreCreateMutex();
        if (spi_mutex == NULL) {
            return 0;
        }
    }
    
    gpio_config_t cs_config = {
        .pin_bit_mask = (1ULL << FONTCS),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&cs_config);
    gpio_set_level(FONTCS, 0);  // CS low (inactive) - Arduino style
    
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
    } else if (ret == ESP_ERR_INVALID_STATE) {
    } else {
        return 0;
    }
    
    spi_device_interface_config_t dev_config = {
        .clock_speed_hz = SPI_FREQUENCY,
        .mode = SPI_MODE,
        .spics_io_num = -1,  // We'll handle CS manually
        .queue_size = 1,
        .flags = 0,
    };
    
    ret = spi_bus_add_device(SPI_HOST, &dev_config, &spi_device);
    if (ret != ESP_OK) {
        return 0;
    }
    
    spi_initialized = true;
    return 1;
}

// 已确认SPI read data function
static unsigned char k10_spi_read_data(unsigned char* sendbuf, unsigned char sendlen, 
                                      unsigned char* receivebuf, unsigned int receivelen) {
    if (!spi_initialized || spi_device == NULL) {
        return 0;
    }
    
    xSemaphoreTake(spi_mutex, portMAX_DELAY);
    
    // CS high (active) - Arduino style
    gpio_set_level(FONTCS, 1);
    
    // Send command and address
    if (sendlen > 0) {
        
        spi_transaction_t trans = {
            .length = sendlen * 8,  // Length in bits
            .tx_buffer = sendbuf,
            .rx_buffer = NULL,
        };
        esp_err_t ret = spi_device_transmit(spi_device, &trans);
        if (ret != ESP_OK) {
            gpio_set_level(FONTCS, 1);  // CS high (inactive)
            xSemaphoreGive(spi_mutex);
            return 0;
        }
    }
    
    // Receive data
    if (receivelen > 0) {
        spi_transaction_t trans = {
            .length = receivelen * 8,  // Length in bits
            .tx_buffer = NULL,
            .rx_buffer = receivebuf,
        };
        esp_err_t ret = spi_device_transmit(spi_device, &trans);
        if (ret != ESP_OK) {
            gpio_set_level(FONTCS, 1);  // CS high (inactive)
            xSemaphoreGive(spi_mutex);
            return 0;
        }
    }
    // CS low (inactive) - Arduino style
    gpio_set_level(FONTCS, 0);
    
    xSemaphoreGive(spi_mutex);
    return 1;
}

// 已确认Read data batch from font chip
static unsigned long k10_read_data_batch(unsigned long address, unsigned long DataLen, unsigned char *pBuff) {
    if (!spi_initialized || spi_device == NULL) {
        return 0;
    }
    
    
    xSemaphoreTake(spi_mutex, portMAX_DELAY);
    
    // CS high (active) - Arduino style
    gpio_set_level(FONTCS, 1);
    // Send read command (0x03) and 24-bit address
    unsigned char cmd[4];
    cmd[0] = 0x03;  // Read command
    cmd[1] = (unsigned char)((address) >> 16);
    cmd[2] = (unsigned char)((address) >> 8);
    cmd[3] = (unsigned char)address;
    
    spi_transaction_t trans = {
        .length = 4 * 8,  // 4 bytes in bits
        .tx_buffer = cmd,
        .rx_buffer = NULL,
    };
    
    esp_err_t ret = spi_device_transmit(spi_device, &trans);
    if (ret != ESP_OK) {
        gpio_set_level(FONTCS, 1);  // CS high (inactive)
        xSemaphoreGive(spi_mutex);
        return 0;
    }
    
    // Read data
    if (DataLen > 0) {
        // Clear buffer before reading
        memset(pBuff, 0x00, DataLen);
        spi_transaction_t trans = {
            .length = DataLen * 8,  // Length in bits
            .tx_buffer = NULL,
            .rx_buffer = pBuff,
        };
        
        ret = spi_device_transmit(spi_device, &trans);
        if (ret != ESP_OK) {
            gpio_set_level(FONTCS, 1);  // CS high (inactive)
            xSemaphoreGive(spi_mutex);
            return 0;
        }
    }
    // CS low (inactive) - Arduino style
    gpio_set_level(FONTCS, 0);
    xSemaphoreGive(spi_mutex);
    return pBuff[0];
}

// 已确认
// Read data batch from font chip (public interface)
unsigned long r_dat_bat(unsigned long address, unsigned long DataLen, unsigned char *pBuff) {
    return k10_read_data_batch(address, DataLen, pBuff);
}

// SPI communication function (public interface)
unsigned char gt_read_data(unsigned char* sendbuf, unsigned char sendlen, 
                          unsigned char* receivebuf, unsigned int receivelen) {
    return k10_spi_read_data(sendbuf, sendlen, receivebuf, receivelen);
}
/*-----------------
 *  FONT CHIP FUNCTIONS
 *----------------*/

// Initialize font chip
int GT_Font_Init(void) {
    
    int result = k10_spi_init();
    return result;
}

// Get ASCII character data from font chip
unsigned char ASCII_GetData(unsigned char asc, unsigned long ascii_kind, unsigned char *DZ_Data) {
    if ( asc <= 0x1Fu || asc > 0x7Eu )
        return 0;
    switch ( ascii_kind ){
    case 1u:
        r_dat_bat(8 * (asc + 259932), 8u, DZ_Data);
    break;
    case 2u:
        r_dat_bat(8 * (asc + 260028), 8u, DZ_Data);
        break;
    case 3u:
        r_dat_bat(12 * asc + 2080864, 0xCu, DZ_Data);
        break;
    case 4u:
        r_dat_bat(26 * asc + 2090144 + 2, 0x18u, DZ_Data);
        break;
    case 5u:
        r_dat_bat(16 * (asc + 130174), 0x10u, DZ_Data);
        break;
    case 6u:
        r_dat_bat(48 * asc + 1543800, 0x30u, DZ_Data);
        break;
    case 7u:
        r_dat_bat(48 * asc + 1549944, 0x30u, DZ_Data);
        break;
    case 8u:
        r_dat_bat(((asc + 67108832) << 6) + 2084832, 0x40u, DZ_Data);
        break;
    case 9u:
        r_dat_bat(34 * asc + 2092384 +2, 0x20u, DZ_Data);
        break;
    case 0xAu:
        r_dat_bat(74 * asc + 1559864 + 2, 0x48u, DZ_Data);
        break;
    case 0xBu:
        r_dat_bat(130 * asc + 2092576, 0x82u, DZ_Data);
        break;
    default:
        return 1;
    }
return 1;
}

// Get character interval/spacing (optional)
unsigned char ASCII_GetInterval(unsigned char asc, unsigned long ascii_kind) {
    unsigned char data[2];
    if ( asc <= 0x1Fu || asc > 0x7Eu )
        return 0;
    switch ( ascii_kind )
    {
    case 1u:
      r_dat_bat(8 * (asc + 259932), 2u, data);
      break;
    case 2u:
      r_dat_bat(8 * (asc + 260028), 2u, data);
      break;
    case 3u:
      r_dat_bat(12 * asc + 2080864, 2u, data);
      break;
    case 4u:
      r_dat_bat(26 * asc + 2090144 , 2u, data);
      break;
    case 5u:
      r_dat_bat(16 * (asc + 130174), 2u, data);
      break;
    case 6u:
      r_dat_bat(48 * asc + 1543800, 2u, data);
      break;
    case 7u:
      r_dat_bat(48 * asc + 1549944, 2u, data);
      break;
    case 8u:
      r_dat_bat(((asc + 67108832) << 6) + 2084832, 2u, data);
      break;
    case 9u:
      r_dat_bat(34 * asc + 2092384, 2u, data);
      break;
    case 0xAu:
      r_dat_bat(74 * asc + 1559864 , 2u, data);
      break;
    case 0xBu:
      r_dat_bat(130 * asc + 2092576, 2u, data);
      break;
    default:
      break;
    }
    return data[1];
}

// Get GBK character data (for Chinese characters)
unsigned long GBK_24_GetData(unsigned char c1, unsigned char c2, unsigned char *DZ_Data) {

    unsigned char temp; 
    int address;
    
    temp = c2;
    address = 0;
    if ( c2 == 127 )
      address = 0;
    if ( c1 <= 0xA0u || c1 > 0xA3u || c2 <= 0xA0u )
    {
      if ( c1 != 166 || c2 <= 0xA0u )
      {
        if ( c1 == 169 && c2 > 0xA0u )
          address = 94 * c1 + c2 - 15671;
      }
      else
      {
        address = 94 * c1 + c2 - 15483;
      }
    }
    else
    {
      address = 94 * c1 + c2 - 15295;
    }
    if ( c1 <= 0xAFu || c1 > 0xF7u || c2 <= 0xA0u )
    {
      if ( c1 > 0xA0u || c1 <= 0x80u || c2 <= 0x3Fu )
      {
        if ( c1 > 0xA9u && c2 <= 0xA0u )
        {
          if ( (c2 & 0x80u) != 0 )
            temp = c2 - 1;
          address = 96 * c1 + temp - 3081;
        }
      }
      else
      {
        if ( (c2 & 0x80u) != 0 )
          temp = c2 - 1;
        address = 190 * c1 + temp - 17351;
      }
    }
    else
    {
      address = 94 * c1 + c2 - 16250;
    }
    r_dat_bat(72 * address, 0x48u, DZ_Data);
    return 72 * address;
}

// Unicode to GBK conversion (from Arduino implementation)
unsigned long U2G(unsigned int unicode) {
    unsigned char pBuff[2];
    int offset;
    unsigned int address = 0;
    
    offset = 2517590;
    
    // Complex Unicode to GBK conversion (from Arduino code)
    if (unicode > 0x451 || unicode <= 0x9F) {
        if (unicode > 0x2642 || unicode <= 0x200F) {
            if (unicode > 0x33D5 || unicode < 0x3000) {
                if (unicode > 0x9FA5 || unicode < 0x4E00) {
                    if (unicode > 0xFE6B || unicode <= 0xFE2F) {
                        if (unicode > 0xFF5E || unicode <= 0xFF00) {
                            if (unicode > 0xFFE5 || unicode <= 0xFFDF) {
                                if (unicode > 0xFA29 || unicode <= 0xF92B) {
                                    if (unicode > 0xE864 || unicode <= 0xE815) {
                                        if (unicode > 0x2ECA || unicode <= 0x2E80) {
                                            if (unicode > 0x49B7 || unicode <= 0x4946) {
                                                if (unicode > 0x4DAE || unicode <= 0x4C76) {
                                                    if (unicode > 0x3CE0 || unicode <= 0x3446) {
                                                        if (unicode <= 0x478D && unicode > 0x4054) {
                                                            address = 2 * (unicode + 2147467178) + 55380;
                                                        }
                                                    } else {
                                                        address = 2 * (unicode + 2147470265) + 50976;
                                                    }
                                                } else {
                                                    address = 2 * (unicode + 2147464073) + 50352;
                                                }
                                            } else {
                                                address = 2 * (unicode + 2147464889) + 50126;
                                            }
                                        } else {
                                            address = 2 * (unicode + 2147471743) + 49978;
                                        }
                                    } else {
                                        address = 2 * (unicode + 2147424234) + 49820;
                                    }
                                } else {
                                    address = 2 * (unicode + 2147419860) + 49312;
                                }
                            } else {
                                address = 2 * (unicode + 2147418144) + 49142;
                            }
                        } else {
                            address = 2 * (unicode + 2147418367) + 48954;
                        }
                    } else {
                        address = 2 * (unicode + 2147418576) + 48834;
                    }
                } else {
                    address = 2 * (unicode + 2147463680) + 7030;
                }
            } else {
                address = 2 * (unicode + 2147471360) + 5066;
            }
        } else {
            address = 2 * (unicode + 2147475440) + 1892;
        }
    } else {
        address = 2 * (unicode + 2147483488);
    }
    
    address += offset;
    
    
    // Read GBK code from font chip
    if (k10_read_data_batch(address, 2, pBuff)) {
        unsigned long gbk_code = (pBuff[0] << 8) | pBuff[1];
        return gbk_code;
    }
    return 0;  // No conversion available
}
static int Uncompress(unsigned char* result, unsigned char *a2)
{
  for ( uint8_t i = 0; i <= 5u; ++i )
  {
    *(uint8_t *)(result + 4 * i) = a2[3 * i];
    *(uint8_t *)(result + 4 * i + 1) = a2[3 * i + 1] & 0xF0;
    *(uint8_t *)(result + 4 * i + 2) = a2[3 * i + 2];
    *(uint8_t *)(result + 4 * i + 3) = 16 * a2[3 * i + 1];
  }
  return 1;
}

// Get 12x12 character data
void gt_12_GetData(unsigned char MSB, unsigned char LSB, unsigned char *DZ_Data) {
    unsigned char pBuff[20];
    int offset;
    unsigned int address = 0; // Initialize address to prevent uninitialized use
    
    offset = 2109216;
      if ( MSB != 169 || LSB <= 0xA3u )
      {
        if ( MSB <= 0xA0u || MSB > 0xA3u || LSB <= 0xA0u )
        {
          if ( MSB > 0xAFu && MSB <= 0xF7u && LSB > 0xA0u )
            address = 18 * (94 * MSB + LSB) + offset - 294246;
        }
        else
        {
          address = 18 * (94 * MSB + LSB) + offset - 275310;
        }
      }
      else
      {
        address = 18 * LSB + offset + 2124;
      }
      r_dat_bat(address, 0x12u, pBuff);
      Uncompress(DZ_Data, pBuff);
}

