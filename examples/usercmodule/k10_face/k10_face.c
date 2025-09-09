#include <string.h>
#include <stdarg.h>
#include "py/nlr.h"
#include "py/obj.h"
#include "py/objtype.h"
#include "py/runtime.h"
#include "py/binary.h"
#include "py/mpstate.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "ai_data.h"
#include "esp_camera.h"


int register_face_flag = 0;
int recognize_face_flag = 0;
int remove_face_flag = 0;
int reset_faces_flag = 0;

void mp_print_face_cstr(const char *str) {
    mp_printf(&mp_plat_print, "%s", str);
}

extern void ai_task(void* arg);
extern void ai_camera_task(void* arg);
extern void init_ai(void);

static mp_obj_t g_ai_callback = mp_const_none;
static QueueHandle_t ai_result_queue = NULL;

// 设置回调
static mp_obj_t ai_set_callback(mp_obj_t callback) {
    if (callback == mp_const_none || mp_obj_is_callable(callback)) {
        g_ai_callback = callback;
    } else {
        mp_raise_ValueError(MP_ERROR_TEXT("callback must be callable or None"));
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(ai_set_callback_obj, ai_set_callback);

// 提供给 C++ 调用，用于把数据塞进队列
void ai_push_result(ai_data_obj_t *data) {
    if (ai_result_queue) {
        ai_data_obj_t copy = *data;
        xQueueSend(ai_result_queue, &copy, 0); // 立即返回，满了就丢弃最新数据
    }
}

// 专门的队列消费任务
static void ai_callback_task(void* arg) {
    ai_data_obj_t data;
    while (1) {
        if (xQueueReceive(ai_result_queue, &data, portMAX_DELAY)) {
            if (g_ai_callback != mp_const_none) {
                // 把结果转成一个简单的 tuple/int，而不是字典
                mp_sched_schedule(
                    g_ai_callback,
                    mp_obj_new_int(data.face_detect.face_id)
                );
            }
        }
    }
}

// 启动任务
static mp_obj_t ai_start_task(void) {
    if (!ai_result_queue) {
        ai_result_queue = xQueueCreate(10, sizeof(ai_data_obj_t)); // 最多缓存10个结果
    }
    xTaskCreatePinnedToCore(ai_task, "ai_task", 1024*8, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(ai_callback_task, "ai_cb_task", 4096, NULL, 5, NULL, 1);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(ai_start_task_obj, ai_start_task);


static mp_obj_t camera_start_task(void) {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer = LEDC_TIMER_0;
    config.pin_d0 = CAMERA_PIN_D0;
    config.pin_d1 = CAMERA_PIN_D1;
    config.pin_d2 = CAMERA_PIN_D2;
    config.pin_d3 = CAMERA_PIN_D3;
    config.pin_d4 = CAMERA_PIN_D4;
    config.pin_d5 = CAMERA_PIN_D5;
    config.pin_d6 = CAMERA_PIN_D6;
    config.pin_d7 = CAMERA_PIN_D7;
    config.pin_xclk = CAMERA_PIN_XCLK;
    config.pin_pclk = CAMERA_PIN_PCLK;
    config.pin_vsync = CAMERA_PIN_VSYNC;
    config.pin_href = CAMERA_PIN_HREF;
    config.pin_sscb_sda = CAMERA_PIN_SIOD;
    config.pin_sscb_scl = CAMERA_PIN_SIOC;
    config.pin_pwdn = CAMERA_PIN_PWDN;
    config.pin_reset = CAMERA_PIN_RESET;
    config.xclk_freq_hz = XCLK_FREQ_HZ;//XCLK_FREQ_HZ
    config.pixel_format = PIXFORMAT_RGB565;
    config.frame_size = FRAMESIZE_QVGA;
    config.jpeg_quality = 5;
    config.fb_count = 2; // 减少缓冲区数量，节省内存

    config.fb_location = CAMERA_FB_IN_PSRAM;
    config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK)
    {
        char error_msg[100];
        snprintf(error_msg, sizeof(error_msg), "Camera init failed with error 0x%x\n", err);
        mp_print_face_cstr(error_msg);
    }
    xTaskCreatePinnedToCore(ai_camera_task, "ai_camera_task", 4096, NULL, 4, NULL, 0);
    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_0(camera_start_task_obj, camera_start_task);


static mp_obj_t register_face(void) {
    register_face_flag = 1;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(register_face_obj, register_face);

static mp_obj_t recognize_face(void) {
    recognize_face_flag = 1;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(recognize_face_obj, recognize_face);


static mp_obj_t remove_face(void) {
    remove_face_flag = 1;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(remove_face_obj, remove_face);


static mp_obj_t reset_faces(void) {
    reset_faces_flag = 1;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(reset_faces_obj, reset_faces);

static mp_obj_t mp_init_ai(void) {
    init_ai();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_init_ai_obj, mp_init_ai);

static const mp_rom_map_elem_t k10_ai_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_ai) },
    { MP_ROM_QSTR(MP_QSTR_init_ai), MP_ROM_PTR(&mp_init_ai_obj) },//人脸识别初始化
    { MP_ROM_QSTR(MP_QSTR_ai_set_callback), MP_ROM_PTR(&ai_set_callback_obj) },
    { MP_ROM_QSTR(MP_QSTR_ai_start_task), MP_ROM_PTR(&ai_start_task_obj) },
    { MP_ROM_QSTR(MP_QSTR_camera_start_task), MP_ROM_PTR(&camera_start_task_obj) },
    { MP_ROM_QSTR(MP_QSTR_register_face), MP_ROM_PTR(&register_face_obj) },//注册人脸
    { MP_ROM_QSTR(MP_QSTR_recognize_face), MP_ROM_PTR(&recognize_face_obj) },//识别人脸
    { MP_ROM_QSTR(MP_QSTR_remove_face), MP_ROM_PTR(&remove_face_obj) },//删除人脸
    { MP_ROM_QSTR(MP_QSTR_reset_faces), MP_ROM_PTR(&reset_faces_obj) },//删除全部人脸
};

static MP_DEFINE_CONST_DICT(k10_ai_globals, k10_ai_globals_table);

const mp_obj_module_t mp_k10_ai_system = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&k10_ai_globals,
};

MP_REGISTER_MODULE(MP_QSTR_ai, mp_k10_ai_system);