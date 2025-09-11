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

extern void face_recognize_start_task(void* arg);
extern void camera_start_task(void* arg);
extern void init_ai(void);
extern void cat_detect_task(void* arg);
extern void code_scanner_task(void* arg);
extern void move_detect_task(void* arg);
extern QueueHandle_t camera_queue;

static mp_obj_t g_ai_callback = mp_const_none;
static QueueHandle_t result_queue = NULL;
static TaskHandle_t ai_callback_task_handle = NULL;
static TaskHandle_t face_recognize_task_handle = NULL;
static TaskHandle_t camera_start_task_handle = NULL;
static TaskHandle_t cat_detect_task_handle = NULL;
static TaskHandle_t code_scanner_task_handle = NULL;
static TaskHandle_t move_detect_task_handle = NULL;
static QueueHandle_t camera_output_queue = NULL;
static ai_data_obj_t g_latest_ai_data;
static bool g_ai_data_updated = false;
static int init_ai_flag = 0;
int free_camera_flag = 0;
int free_ai_flag = 0;

// 设置回调
static mp_obj_t mp_set_ai_callback(mp_obj_t callback) {
    if (callback == mp_const_none || mp_obj_is_callable(callback)) {
        g_ai_callback = callback;
    } else {
        mp_raise_ValueError(MP_ERROR_TEXT("callback must be callable or None"));
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_1(mp_set_ai_callback_obj, mp_set_ai_callback);

// 提供给 C++ 调用，用于把数据塞进队列
void ai_push_result(ai_data_obj_t *data) {
    if (result_queue) {
        ai_data_obj_t copy = *data;
        xQueueSend(result_queue, &copy, 0); // 立即返回，满了就丢弃最新数据
    }
}

void camera_push_result(camera_fb_t *data) {
    if (camera_output_queue) {
        xQueueSend(camera_output_queue, &data, 0); // 传递指针，立即返回，满了就丢弃最新数据
    }
}

// 专门的队列消费任务
static void ai_callback_task(void* arg) {
    ai_data_obj_t data;
    while (1) {
        if (free_ai_flag == 1) {
            break;
        }
        if (xQueueReceive(result_queue, &data, portMAX_DELAY)) {
            // 更新全局数据
            g_latest_ai_data = data;
            g_ai_data_updated = true;
            
            if (g_ai_callback != mp_const_none) {
                // 只发送简单的通知，不发送数据
                mp_sched_schedule(g_ai_callback, mp_const_none);
            }
        }
    }
    vTaskDelete(NULL);
}




static mp_obj_t mp_camera_start(void) {
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
    xTaskCreatePinnedToCore(camera_start_task, "camera_start_task", 4096, NULL, 4, &camera_start_task_handle, 0);
    return mp_const_none;
}

static MP_DEFINE_CONST_FUN_OBJ_0(mp_camera_start_obj, mp_camera_start);

// 启动任务
static mp_obj_t mp_face_recognize_start(void) {
    init_ai_flag = 1;
    xTaskCreatePinnedToCore(face_recognize_start_task, "face_recognize_start_task", 1024*8, NULL, 4, &face_recognize_task_handle, 0);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_face_recognize_start_obj, mp_face_recognize_start);

static mp_obj_t mp_register_face(void) {
    register_face_flag = 1;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_register_face_obj, mp_register_face);

static mp_obj_t mp_recognize_face(void) {
    recognize_face_flag = 1;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_recognize_face_obj, mp_recognize_face);


static mp_obj_t mp_remove_face(void) {
    remove_face_flag = 1;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_remove_face_obj, mp_remove_face);


static mp_obj_t mp_reset_faces(void) {
    reset_faces_flag = 1;
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_reset_faces_obj, mp_reset_faces);

static mp_obj_t mp_init_ai(void) {
    init_ai_flag = 0;
    free_ai_flag = 0;
    free_camera_flag = 0;
    if (!result_queue) {
        result_queue = xQueueCreate(10, sizeof(ai_data_obj_t)); // 最多缓存10个结果
    }
    // 创建AI回调任务
    //if (!ai_callback_task_handle) {
        xTaskCreatePinnedToCore(ai_callback_task, "ai_cb_task", 1024*4, NULL, 5, &ai_callback_task_handle, 1);
    //}
    if (!camera_output_queue) {
        camera_output_queue = xQueueCreate(5, sizeof(camera_fb_t *)); // 最多缓存5个帧指针
    }
    
    init_ai();
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_init_ai_obj, mp_init_ai);

static mp_obj_t mp_cat_detect(void) {
    init_ai_flag = 1;
    xTaskCreatePinnedToCore(cat_detect_task, "cat_detect_task", 1024*8, NULL, 4, &cat_detect_task_handle, 0);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_cat_detect_obj, mp_cat_detect);

static mp_obj_t mp_code_scanner(void) {
    init_ai_flag = 1;
    xTaskCreatePinnedToCore(code_scanner_task, "code_scanner_task", 1024*8, NULL, 3, &code_scanner_task_handle, 0);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_code_scanner_obj, mp_code_scanner);

static mp_obj_t mp_move_detect(void) {
    init_ai_flag = 1;
    xTaskCreatePinnedToCore(move_detect_task, "move_detect_task", 1024*8, NULL, 4, &move_detect_task_handle, 0);
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_move_detect_obj, mp_move_detect);

static mp_obj_t mp_camera_capture(void) {
    camera_fb_t *frame = NULL;
    if (init_ai_flag == 0) {
        if (xQueueReceive(camera_queue, &frame, 0)) { // 非阻塞接收
            if (frame) {
                mp_obj_t image = mp_obj_new_bytes(frame->buf, frame->len);
                esp_camera_fb_return(frame); // 释放帧缓冲区
                if (image != mp_const_none) {
                    return image;
                }
                // 如果创建bytes对象失败，返回None
            }
        }
    }else{
        if (xQueueReceive(camera_output_queue, &frame, 0)) { // 非阻塞接收
            if (frame) {
                mp_obj_t image = mp_obj_new_bytes(frame->buf, frame->len);
                esp_camera_fb_return(frame); // 释放帧缓冲区
                if (image != mp_const_none) {
                    return image;
                }
                // 如果创建bytes对象失败，返回None
            }
        }
    }
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_camera_capture_obj, mp_camera_capture);

// 获取完整的AI数据
static mp_obj_t mp_get_ai_data(void) {
    mp_obj_t result_dict = mp_obj_new_dict(0);
    
    // 添加基本标志
    mp_obj_dict_store(result_dict, MP_OBJ_NEW_QSTR(MP_QSTR_move_flag), mp_obj_new_bool(g_latest_ai_data.move_flag));
    mp_obj_dict_store(result_dict, MP_OBJ_NEW_QSTR(MP_QSTR_face_flag), mp_obj_new_bool(g_latest_ai_data.face_flag));
    mp_obj_dict_store(result_dict, MP_OBJ_NEW_QSTR(MP_QSTR_cat_flag), mp_obj_new_bool(g_latest_ai_data.cat_flag));
    mp_obj_dict_store(result_dict, MP_OBJ_NEW_QSTR(MP_QSTR_code_flag), mp_obj_new_bool(g_latest_ai_data.code_flag));
    
    // 添加人脸检测数据（总是创建，即使没有检测到人脸）
    mp_obj_t face_dict = mp_obj_new_dict(0);
    mp_obj_dict_store(face_dict, MP_OBJ_NEW_QSTR(MP_QSTR_face_id), mp_obj_new_int(g_latest_ai_data.face_detect.face_id));
    mp_obj_dict_store(face_dict, MP_OBJ_NEW_QSTR(MP_QSTR_frame_length), mp_obj_new_int(g_latest_ai_data.face_detect.face_frame_length));
    mp_obj_dict_store(face_dict, MP_OBJ_NEW_QSTR(MP_QSTR_frame_width), mp_obj_new_int(g_latest_ai_data.face_detect.face_frame_width));
    
    // 添加面部特征点坐标
    mp_obj_t left_eye = mp_obj_new_tuple(2, (mp_obj_t[]){mp_obj_new_int(g_latest_ai_data.face_detect.face_left_eys[0]), mp_obj_new_int(g_latest_ai_data.face_detect.face_left_eys[1])});
    mp_obj_t right_eye = mp_obj_new_tuple(2, (mp_obj_t[]){mp_obj_new_int(g_latest_ai_data.face_detect.face_right_eys[0]), mp_obj_new_int(g_latest_ai_data.face_detect.face_right_eys[1])});
    mp_obj_t nose = mp_obj_new_tuple(2, (mp_obj_t[]){mp_obj_new_int(g_latest_ai_data.face_detect.face_nose[0]), mp_obj_new_int(g_latest_ai_data.face_detect.face_nose[1])});
    mp_obj_t left_mouth = mp_obj_new_tuple(2, (mp_obj_t[]){mp_obj_new_int(g_latest_ai_data.face_detect.face_left_mouth[0]), mp_obj_new_int(g_latest_ai_data.face_detect.face_left_mouth[1])});
    mp_obj_t right_mouth = mp_obj_new_tuple(2, (mp_obj_t[]){mp_obj_new_int(g_latest_ai_data.face_detect.face_right_mouth[0]), mp_obj_new_int(g_latest_ai_data.face_detect.face_right_mouth[1])});
    
    mp_obj_dict_store(face_dict, MP_OBJ_NEW_QSTR(MP_QSTR_left_eye), left_eye);
    mp_obj_dict_store(face_dict, MP_OBJ_NEW_QSTR(MP_QSTR_right_eye), right_eye);
    mp_obj_dict_store(face_dict, MP_OBJ_NEW_QSTR(MP_QSTR_nose), nose);
    mp_obj_dict_store(face_dict, MP_OBJ_NEW_QSTR(MP_QSTR_left_mouth), left_mouth);
    mp_obj_dict_store(face_dict, MP_OBJ_NEW_QSTR(MP_QSTR_right_mouth), right_mouth);
    
    mp_obj_dict_store(result_dict, MP_OBJ_NEW_QSTR(MP_QSTR_face_detect), face_dict);
    
    // 添加猫咪检测数据（总是创建，即使没有检测到猫咪）
    mp_obj_t cat_dict = mp_obj_new_dict(0);
    mp_obj_dict_store(cat_dict, MP_OBJ_NEW_QSTR(MP_QSTR_frame_length), mp_obj_new_int(g_latest_ai_data.cat_detect.cat_frame_length));
    mp_obj_dict_store(cat_dict, MP_OBJ_NEW_QSTR(MP_QSTR_frame_width), mp_obj_new_int(g_latest_ai_data.cat_detect.cat_frame_width));
    mp_obj_dict_store(result_dict, MP_OBJ_NEW_QSTR(MP_QSTR_cat_detect), cat_dict);
    
    // 添加二维码数据（总是创建，即使没有检测到二维码）
    if (g_latest_ai_data.code_data != NULL) {
        mp_obj_dict_store(result_dict, MP_OBJ_NEW_QSTR(MP_QSTR_code_data), mp_obj_new_str(g_latest_ai_data.code_data, strlen(g_latest_ai_data.code_data)));
    } else {
        mp_obj_dict_store(result_dict, MP_OBJ_NEW_QSTR(MP_QSTR_code_data), mp_const_none);
    }
    
    return result_dict;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_get_ai_data_obj, mp_get_ai_data);

// 检查AI数据是否已更新
static mp_obj_t mp_is_ai_data_updated(void) {
    bool updated = g_ai_data_updated;
    g_ai_data_updated = false; // 重置标志
    return mp_obj_new_bool(updated);
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_is_ai_data_updated_obj, mp_is_ai_data_updated);

// 释放AI系统和资源
static mp_obj_t mp_deinit_ai(void) {
    free_ai_flag = 1;
    
    // 删除摄像头输出队列
    if (camera_output_queue != NULL) {
        // 清空队列中剩余的帧
        camera_fb_t *frame = NULL;
        while (xQueueReceive(camera_output_queue, &frame, 0) == pdPASS) {
            if (frame) {
                esp_camera_fb_return(frame);
            }
        }
        vQueueDelete(camera_output_queue);
        camera_output_queue = NULL;
    }
    
    // 重置回调函数
    g_ai_callback = mp_const_none;
    
    // 重置数据更新标志
    g_ai_data_updated = false;
    
    // 清零AI数据
    memset(&g_latest_ai_data, 0, sizeof(g_latest_ai_data));
    
    
    
    return mp_const_none;
}
static MP_DEFINE_CONST_FUN_OBJ_0(mp_deinit_ai_obj, mp_deinit_ai);


static const mp_rom_map_elem_t k10_ai_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_ai) },
    { MP_ROM_QSTR(MP_QSTR_init_ai), MP_ROM_PTR(&mp_init_ai_obj) },//人脸识别初始化
    { MP_ROM_QSTR(MP_QSTR_deinit_ai), MP_ROM_PTR(&mp_deinit_ai_obj) },//释放AI系统资源
    { MP_ROM_QSTR(MP_QSTR_set_ai_callback), MP_ROM_PTR(&mp_set_ai_callback_obj) },//设置AI回调
    { MP_ROM_QSTR(MP_QSTR_camera_start), MP_ROM_PTR(&mp_camera_start_obj) },//初始化摄像头
    { MP_ROM_QSTR(MP_QSTR_face_recognize_start), MP_ROM_PTR(&mp_face_recognize_start_obj) },//启动人脸识别
    { MP_ROM_QSTR(MP_QSTR_register_face), MP_ROM_PTR(&mp_register_face_obj) },//注册人脸
    { MP_ROM_QSTR(MP_QSTR_recognize_face), MP_ROM_PTR(&mp_recognize_face_obj) },//识别人脸
    { MP_ROM_QSTR(MP_QSTR_remove_face), MP_ROM_PTR(&mp_remove_face_obj) },//删除人脸
    { MP_ROM_QSTR(MP_QSTR_reset_faces), MP_ROM_PTR(&mp_reset_faces_obj) },//删除全部人脸
    { MP_ROM_QSTR(MP_QSTR_cat_detect), MP_ROM_PTR(&mp_cat_detect_obj) },//猫脸检测
    { MP_ROM_QSTR(MP_QSTR_code_scanner), MP_ROM_PTR(&mp_code_scanner_obj) },//二维码扫描
    { MP_ROM_QSTR(MP_QSTR_move_detect), MP_ROM_PTR(&mp_move_detect_obj) },//移动检测
    { MP_ROM_QSTR(MP_QSTR_camera_capture), MP_ROM_PTR(&mp_camera_capture_obj) },//获取摄像头图像
    { MP_ROM_QSTR(MP_QSTR_get_ai_data), MP_ROM_PTR(&mp_get_ai_data_obj) },//获取完整AI数据
    { MP_ROM_QSTR(MP_QSTR_is_ai_data_updated), MP_ROM_PTR(&mp_is_ai_data_updated_obj) },//检查AI数据是否更新
};

static MP_DEFINE_CONST_DICT(k10_ai_globals, k10_ai_globals_table);

const mp_obj_module_t mp_k10_ai_system = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&k10_ai_globals,
};

MP_REGISTER_MODULE(MP_QSTR_ai, mp_k10_ai_system);