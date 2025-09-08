// asr_data.h
#ifndef ASR_DATA_H
#define ASR_DATA_H

#include "py/obj.h"
#include <stdbool.h>
#include "driver/gpio.h"

//GPIO_NUM_27
#define IIS_BLCK            GPIO_NUM_0
#define IIS_LRCK            GPIO_NUM_38
#define IIS_DSIN            GPIO_NUM_39
#define IIS_DOUT            GPIO_NUM_45
#define IIS_MCLK            GPIO_NUM_3

#define I2C_MASTER_SCL_IO    GPIO_NUM_48
#define I2C_MASTER_SDA_IO    GPIO_NUM_47
#define I2C_MASTER_NUM       I2C_NUM_0
#define I2C_MASTER_FREQ_HZ 100000
#define I2C_DEVICE_ADDR_1 0x15
#define I2C_DEVICE_ADDR_2 0x11
#define I2C_MASTER_TX_BUF_DISABLE 0
#define I2C_MASTER_RX_BUF_DISABLE 0
#define I2C_TIMEOUT_MS      1000
#define XL9555_ADDR          0x20  // 7-bit 地址 (A0~A2=0)

#define  ASR_MODE_SINGLE            1
#define  ASR_MODE_CONTINUOUS        2

// 结构体定义
typedef struct {
    uint8_t reg;
    uint8_t val;
} reg_cfg_t;

typedef struct _asr_data_obj_t {
    mp_obj_base_t base;
    
    // 音频相关数据
    int audio_id;           // 语音识别ID
    
} asr_data_obj_t;

#ifdef __cplusplus
extern "C" {
#endif

// 函数声明
void mp_print_asr_cstr(const char *str);
void mp_print_asr(const char *fmt, ...);
extern int audio_capture_flag;

// ES7243E配置表声明
extern reg_cfg_t es7243e_stop_table[];
extern reg_cfg_t es7243e_config_table[];
extern reg_cfg_t es7243e_start_table[];

#ifdef __cplusplus
}
#endif

#endif // ASR_DATA_H
