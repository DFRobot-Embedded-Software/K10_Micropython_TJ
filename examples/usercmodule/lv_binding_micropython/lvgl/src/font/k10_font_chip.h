/*******************************************************************************
 * K10 Font Chip Driver - GT30L24A3W Header
 * Simplified version for testing
 ******************************************************************************/

#ifndef _K10_FONT_CHIP_H_
#define _K10_FONT_CHIP_H_

#include <stdint.h>

/*-----------------
 *  FONT CHIP CONFIG
 *----------------*/

#define FONTCS                 40

// Font format definitions
#define ASCII_5X7              1
#define ASCII_7X8              2
#define ASCII_6X12             3
#define ASCII_12_A             4 //12 * 16
#define ASCII_8X16             5
#define ASCII_12X24_A          6
#define ASCII_12X24_P          7
#define ASCII_16X32            8
#define ASCII_16_A             9
#define ASCII_24_B            10
#define ASCII_32_B            11

/*-----------------
 *  FUNCTION DECLARATIONS
 *----------------*/

// Initialize font chip
int GT_Font_Init(void);

// Get ASCII character data
unsigned char ASCII_GetData(unsigned char asc, unsigned long ascii_kind, unsigned char *DZ_Data);

// Get character interval/spacing
unsigned char ASCII_GetInterval(unsigned char asc, unsigned long ascii_kind);

// Get GBK character data (for Chinese characters)
unsigned long GBK_24_GetData(unsigned char c1, unsigned char c2, unsigned char *DZ_Data);

// Unicode to GBK conversion
unsigned long U2G(unsigned int unicode);

// Get 12x12 character data
void gt_12_GetData(unsigned char MSB, unsigned char LSB, unsigned char *DZ_Data);

// Read data batch from font chip
unsigned long r_dat_bat(unsigned long address, unsigned long DataLen, unsigned char *pBuff);

// SPI communication function
unsigned char gt_read_data(unsigned char* sendbuf, unsigned char sendlen, unsigned char* receivebuf, unsigned int receivelen);

#endif /* _K10_FONT_CHIP_H_ */
