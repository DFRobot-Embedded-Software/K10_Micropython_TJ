// ai_data_obj_t.h
#ifndef AI_DATA_OBJ_H
#define AI_DATA_OBJ_H

#include "py/obj.h"
#include <stdbool.h>

typedef struct _ai_data_obj_t {
    mp_obj_base_t base;

    struct {
        int face_id;
        int face_frame_length;
        int face_frame_width;
        int face_left_eys[2];
        int face_right_eys[2];
        int face_nose[2];
        int face_left_mouth[2];
        int face_right_mouth[2];
    } face_detect;

    struct {
        int cat_frame_length;
        int cat_frame_width;
    } cat_detect;

    const char* code_data;
    bool move_flag;
    bool face_flag;
    bool cat_flag;
    bool code_flag;
} ai_data_obj_t;


#define CAMERA_PIN_PWDN -1
#define CAMERA_PIN_RESET -1
#define CAMERA_PIN_XCLK 7
#define CAMERA_PIN_SIOD 47
#define CAMERA_PIN_SIOC 48

#define CAMERA_PIN_D7 6
#define CAMERA_PIN_D6 15
#define CAMERA_PIN_D5 16
#define CAMERA_PIN_D4 18
#define CAMERA_PIN_D3 9
#define CAMERA_PIN_D2 11
#define CAMERA_PIN_D1 10
#define CAMERA_PIN_D0 8
#define CAMERA_PIN_VSYNC 4
#define CAMERA_PIN_HREF 5
#define CAMERA_PIN_PCLK 17
#define XCLK_FREQ_HZ 15000000

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

#ifdef __cplusplus
extern "C" {
#endif
void mp_print_face_cstr(const char *str);
extern int register_face_flag;
#ifdef __cplusplus
}
#endif

#endif
