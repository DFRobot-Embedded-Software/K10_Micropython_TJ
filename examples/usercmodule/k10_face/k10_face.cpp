// 在包含任何头文件之前，先处理 ESP-IDF 宏问题

// 定义常量
#ifndef I2C_MASTER_NUM
#define I2C_MASTER_NUM 0
#endif

#ifndef I2C_MASTER_SDA_IO
#define I2C_MASTER_SDA_IO GPIO_NUM_47
#endif

#ifndef I2C_MASTER_SCL_IO
#define I2C_MASTER_SCL_IO GPIO_NUM_48
#endif

#ifndef I2C_MASTER_FREQ_HZ
#define I2C_MASTER_FREQ_HZ 400000
#endif

#ifndef XL9555_ADDR
#define XL9555_ADDR 0x20
#endif

// 首先包含 MicroPython 相关头文件
#include "py/obj.h"
#include "py/runtime.h"
#include "py/mpstate.h"

// 标准库头文件
#include <string>
#include <vector>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

// ESP-IDF 头文件
extern "C" {
    #include "esp_log.h"
    #include "esp_system.h"
    #include "esp_partition.h"
    #include "freertos/FreeRTOS.h"
    #include "freertos/semphr.h"
    #include "freertos/task.h"
    #include "freertos/queue.h"
    #include "esp_camera.h"
    #include "driver/i2c.h"
    #include "driver/gpio.h"
    #include "esp_heap_caps.h"
    #include "soc/soc.h"
    #include "soc/rtc.h"
}

// ESP-DL 头文件
#include "dl_image.hpp"
#include "human_face_detect_msr01.hpp"
#include "human_face_detect_mnp01.hpp"
#include "face_recognition_tool.hpp"
#include "face_recognition_112_v1_s16.hpp"
#include "cat_face_detect_mn03.hpp"
#include "esp_code_scanner.h"
#include "dl_tool.hpp"

// 项目头文件
#include "ai_data.h"



QueueHandle_t camera_queue = NULL;
static face_info_t recognize_result;

// AI推理保护机制
static SemaphoreHandle_t ai_inference_mutex = NULL;
static bool ai_inference_in_progress = false;
static portMUX_TYPE ai_critical_mutex = portMUX_INITIALIZER_UNLOCKED;

// 性能监控变量
static uint32_t ai_inference_count = 0;
static uint32_t ai_inference_total_time = 0;
static uint32_t ai_inference_max_time = 0;

// 内存保护机制
static uint8_t* g_aligned_buffer = NULL;
static size_t g_buffer_size = 0;
static bool g_buffer_initialized = false;

// 初始化对齐内存缓冲区
bool init_aligned_buffer(size_t size) {
    if (g_buffer_initialized && g_buffer_size >= size) {
        return true;
    }
    
    // 释放旧缓冲区
    if (g_aligned_buffer) {
        heap_caps_free(g_aligned_buffer);
        g_aligned_buffer = NULL;
    }
    
    // 分配新的对齐缓冲区：优先内部RAM，失败回退到PSRAM
    g_aligned_buffer = (uint8_t*)heap_caps_aligned_alloc(16, size, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT);
    if (!g_aligned_buffer) {
        g_aligned_buffer = (uint8_t*)heap_caps_aligned_alloc(16, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    }
    if (g_aligned_buffer) {
        g_buffer_size = size;
        g_buffer_initialized = true;
        return true;
    }
    
    return false;
}









// 清理内存资源
extern "C" void cleanup_ai_resources() {
    if (g_aligned_buffer) {
        heap_caps_free(g_aligned_buffer);
        g_aligned_buffer = NULL;
        g_buffer_size = 0;
        g_buffer_initialized = false;
    }
    
    if (ai_inference_mutex) {
        vSemaphoreDelete(ai_inference_mutex);
        ai_inference_mutex = NULL;
    }
    
    // 清理摄像头队列
    if (camera_queue != NULL) {
        // 清空队列中剩余的帧
        camera_fb_t *frame = NULL;
        while (xQueueReceive(camera_queue, &frame, 0) == pdPASS) {
            if (frame) {
                esp_camera_fb_return(frame);
            }
        }
        vQueueDelete(camera_queue);
        camera_queue = NULL;
    }
    
    // 清理I2C驱动
    i2c_driver_delete(I2C_MASTER_NUM);
}

// 安全的清理函数（不删除I2C驱动）
extern "C" void cleanup_ai_resources_safe() {
    
    if (g_aligned_buffer) {
        heap_caps_free(g_aligned_buffer);
        g_aligned_buffer = NULL;
        g_buffer_size = 0;
        g_buffer_initialized = false;
    }
    
    if (ai_inference_mutex) {
        vSemaphoreDelete(ai_inference_mutex);
        ai_inference_mutex = NULL;
    }
    
    // 清理摄像头队列
    if (camera_queue != NULL) {
        // 清空队列中剩余的帧
        camera_fb_t *frame = NULL;
        int frame_count = 0;
        while (xQueueReceive(camera_queue, &frame, 0) == pdPASS) {
            if (frame) {
                esp_camera_fb_return(frame);
                frame_count++;
            }
        }
        vQueueDelete(camera_queue);
        camera_queue = NULL;
    }
    
    // 注意：不删除I2C驱动，避免影响其他功能
}

// 强制清理函数（不等待，直接删除）
extern "C" void cleanup_ai_resources_force() {
    
    // 强制清理内存缓冲区
    if (g_aligned_buffer) {
        heap_caps_free(g_aligned_buffer);
        g_aligned_buffer = NULL;
        g_buffer_size = 0;
        g_buffer_initialized = false;
    }
    
    // 强制清理互斥锁
    if (ai_inference_mutex) {
        vSemaphoreDelete(ai_inference_mutex);
        ai_inference_mutex = NULL;
    }
    
    // 强制清理摄像头队列（不等待，直接删除）
    if (camera_queue != NULL) {
        vQueueDelete(camera_queue);
        camera_queue = NULL;
    }
    
}

extern "C" int register_face_flag;
extern "C" int recognize_face_flag;
extern "C" int remove_face_flag;
extern "C" int reset_faces_flag;
extern "C" int free_camera_flag;
extern "C" int free_ai_flag;
//extern "C" QueueHandle_t camera_queue;
static ai_data_obj_t g_ai_data;

extern "C" void ai_push_result(ai_data_obj_t *data);
extern "C" void camera_push_result(camera_fb_t *data);

esp_err_t xl9555_write_ai(uint8_t reg, uint8_t data) {
    uint8_t buf[2] = {reg, data};
    return i2c_master_write_to_device(I2C_MASTER_NUM, XL9555_ADDR, buf, 2, 1000 / portTICK_PERIOD_MS);
}
esp_err_t xl9555_read_ai(uint8_t reg, uint8_t *data) {
    return i2c_master_write_read_device(I2C_MASTER_NUM, XL9555_ADDR, &reg, 1, data, 1, 1000 / portTICK_PERIOD_MS);
}


static void draw_detection_result(uint16_t *image_ptr, int image_height, int image_width, std::list<dl::detect::result_t> &results)
{
    int i = 0;
    for (std::list<dl::detect::result_t>::iterator prediction = results.begin(); prediction != results.end(); prediction++, i++)
    {
        // 钳制边界，避免越界绘制
        int x0 = DL_MAX(prediction->box[0], 0);
        int y0 = DL_MAX(prediction->box[1], 0);
        int x1 = DL_MAX(prediction->box[2], 0);
        int y1 = DL_MAX(prediction->box[3], 0);
        if (x0 >= image_width) x0 = image_width - 1;
        if (x1 >= image_width) x1 = image_width - 1;
        if (y0 >= image_height) y0 = image_height - 1;
        if (y1 >= image_height) y1 = image_height - 1;
        if (x1 < x0) { int t = x0; x0 = x1; x1 = t; }
        if (y1 < y0) { int t = y0; y0 = y1; y1 = t; }
        dl::image::draw_hollow_rectangle(image_ptr, image_height, image_width,
                                         x0, y0, x1, y1,
                                         0b1110000000000111);
    }

}

// 初始化AI数据结构体
void init_ai_data(ai_data_obj_t *data) {
    // 清零整个结构体
    memset(data, 0, sizeof(ai_data_obj_t));
    
    // 设置默认值
    data->face_flag = false;
    data->cat_flag = false;
    data->code_flag = false;
    data->move_flag = false;
    
    // 初始化人脸检测数据
    data->face_detect.face_id = -1;
    data->face_detect.face_frame_length = 0;
    data->face_detect.face_frame_width = 0;
    
    // 初始化面部特征点坐标
    data->face_detect.face_left_eys[0] = 0;
    data->face_detect.face_left_eys[1] = 0;
    data->face_detect.face_right_eys[0] = 0;
    data->face_detect.face_right_eys[1] = 0;
    data->face_detect.face_nose[0] = 0;
    data->face_detect.face_nose[1] = 0;
    data->face_detect.face_left_mouth[0] = 0;
    data->face_detect.face_left_mouth[1] = 0;
    data->face_detect.face_right_mouth[0] = 0;
    data->face_detect.face_right_mouth[1] = 0;
    
    // 初始化猫咪检测数据
    data->cat_detect.cat_frame_length = 0;
    data->cat_detect.cat_frame_width = 0;
    
    // 初始化二维码数据
    data->code_data = NULL;
}

extern "C" __attribute__((weak)) void init_ai(void)
{
    // I2C 配置
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
    };
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    // 安装驱动
    esp_err_t ret = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (ret == ESP_OK) {
        i2c_driver_install(I2C_MASTER_NUM, conf.mode,
            I2C_MASTER_RX_BUF_DISABLE,
            I2C_MASTER_TX_BUF_DISABLE, 0);
    }

    uint8_t confData0;
    xl9555_read_ai(0x07, &confData0);
    uint8_t confData1;
    xl9555_read_ai(0x06, &confData1);

    // 修改 bit7 为 0（输出）
    confData0 &= ~(1 << 7);
    xl9555_write_ai(0x07, confData0);
    confData1 &= ~(1 << 1);
    xl9555_write_ai(0x06, confData1);
    xl9555_write_ai(0x02, 0x00);
    vTaskDelay(pdMS_TO_TICKS(100));
    xl9555_write_ai(0x02, 0x02);
    vTaskDelay(pdMS_TO_TICKS(100));
    //默认关闭用户灯
    xl9555_write_ai(0x03, 0x00);

    register_face_flag = 0;
    recognize_face_flag = 0;
    remove_face_flag = 0;
    reset_faces_flag = 0;

}

extern "C" __attribute__((weak)) void camera_start_task(void* arg) {
    
    if (!camera_queue) {
        camera_queue = xQueueCreate(10, sizeof(camera_fb_t *)); // 最多缓存10个结果
    }
    
    while (1) {
        if (free_camera_flag == 1) {
            vTaskDelay(pdMS_TO_TICKS(100)); // 等待100ms后重试
            break;
        }
        camera_fb_t *frame = esp_camera_fb_get();
        if (frame){
            xQueueSend(camera_queue, &frame, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    // 注意：不在这里调用esp_camera_deinit()，由mp_deinit_ai统一处理
    vTaskDelete(NULL);
}


extern "C" __attribute__((weak))  void face_recognize_start_task(void* arg) {

    dl::tool::Latency latency;
    HumanFaceDetectMSR01 detectorFace(0.1F, 0.5F, 10, 0.2F);
    HumanFaceDetectMNP01 detectorFace2(0.5F, 0.3F, 5);
    FaceRecognition112V1S16  *recognizer = new FaceRecognition112V1S16();
    recognizer->set_partition(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "fr");
    recognizer->set_ids_from_flash();
    // 初始化AI数据
    init_ai_data(&g_ai_data);

    camera_fb_t *frame = NULL;

    while (1) {
        if (free_ai_flag == 1) {
            break;
        }
        // 使用较短的超时时间，以便能够及时检查退出标志
        if(xQueueReceive(camera_queue, &frame, pdMS_TO_TICKS(100)) == pdPASS) {
            try {
                // 基本帧验证已在上方完成，这里进一步校验尺寸以确保 RGB565 (2 bytes per pixel)
                size_t required_size = (size_t)frame->width * (size_t)frame->height * 2;
                if ((size_t)frame->len < required_size) {
                    esp_camera_fb_return(frame);
                    g_ai_data.face_flag = false;
                    ai_push_result(&g_ai_data);
                    continue;
                }

                // 使用对齐缓冲区承载推理输入，避免直接用DMA帧
                if (!init_aligned_buffer(required_size)) {
                    esp_camera_fb_return(frame);
                    g_ai_data.face_flag = false;
                    ai_push_result(&g_ai_data);
                    continue;
                }
                memcpy(g_aligned_buffer, frame->buf, required_size);

                latency.start();
                std::list<dl::detect::result_t> &detect_candidates = detectorFace.infer((uint16_t*)g_aligned_buffer, {(int)frame->height, (int)frame->width, 3});
                std::list<dl::detect::result_t> &detect_results = detectorFace2.infer((uint16_t*)g_aligned_buffer, {(int)frame->height, (int)frame->width, 3}, detect_candidates);
                latency.end();
                if (detect_results.size() > 0) {
                    g_ai_data.face_flag = true;
                    std::list<dl::detect::result_t>::iterator first_result = detect_results.begin();
                    if (first_result != detect_results.end()) {
                        g_ai_data.face_detect.face_frame_length = (int)first_result->box[2] - (int)first_result->box[0];
                        g_ai_data.face_detect.face_frame_width = (int)first_result->box[3] - (int)first_result->box[1];

                        // 安全地访问 keypoint 数组
                        if (!first_result->keypoint.empty() && first_result->keypoint.size() >= 10) {
                            g_ai_data.face_detect.face_left_eys[0] = (int)first_result->keypoint[0];
                            g_ai_data.face_detect.face_left_eys[1] = (int)first_result->keypoint[1];
                            g_ai_data.face_detect.face_right_eys[0] = (int)first_result->keypoint[6];
                            g_ai_data.face_detect.face_right_eys[1] = (int)first_result->keypoint[7];
                            g_ai_data.face_detect.face_nose[0] = (int)first_result->keypoint[4];
                            g_ai_data.face_detect.face_nose[1] = (int)first_result->keypoint[5];
                            g_ai_data.face_detect.face_left_mouth[0] = (int)first_result->keypoint[2];
                            g_ai_data.face_detect.face_left_mouth[1] = (int)first_result->keypoint[3];
                            g_ai_data.face_detect.face_right_mouth[0] = (int)first_result->keypoint[8];
                            g_ai_data.face_detect.face_right_mouth[1] = (int)first_result->keypoint[9];
                        } else {
                            // 如果关键点数据无效，设置为默认值
                            memset(g_ai_data.face_detect.face_left_eys, 0, sizeof(g_ai_data.face_detect.face_left_eys));
                            memset(g_ai_data.face_detect.face_right_eys, 0, sizeof(g_ai_data.face_detect.face_right_eys));
                            memset(g_ai_data.face_detect.face_nose, 0, sizeof(g_ai_data.face_detect.face_nose));
                            memset(g_ai_data.face_detect.face_left_mouth, 0, sizeof(g_ai_data.face_detect.face_left_mouth));
                            memset(g_ai_data.face_detect.face_right_mouth, 0, sizeof(g_ai_data.face_detect.face_right_mouth));
                        }
                    }

                    // 人脸注册
                    if(register_face_flag == 1){
                        if (!detect_results.empty() && !detect_results.front().keypoint.empty()) {
                            recognizer->enroll_id((uint16_t *)g_aligned_buffer, {(int)frame->height, (int)frame->width, 3}, detect_results.front().keypoint, "", true);
                        }
                        register_face_flag = 0;
                    }
                    // 人脸识别（自动识别或手动触发）
                    if(recognize_face_flag == 1 ){
                        if (!detect_results.empty() && !detect_results.front().keypoint.empty()) {
                            recognize_result = recognizer->recognize((uint16_t *)g_aligned_buffer, {(int)frame->height, (int)frame->width, 3}, detect_results.front().keypoint);
                            g_ai_data.face_detect.face_id = recognize_result.id;
                        } else {
                            g_ai_data.face_detect.face_id = -1;
                        }
                        recognize_face_flag = 0;
                    }

                    // 画框仍在原始帧上，便于显示链路复用
                    draw_detection_result((uint16_t*)frame->buf, (int)frame->height, (int)frame->width, detect_results);
                } else {
                    g_ai_data.face_flag = false;
                    g_ai_data.face_detect.face_id = -1;
                }

                ai_push_result(&g_ai_data);
                camera_push_result(frame);
            } catch (...) {
                if (frame) {
                    esp_camera_fb_return(frame);
                }
                g_ai_data.face_flag = false;
                g_ai_data.face_detect.face_id = -1;
                ai_push_result(&g_ai_data);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10)); // 与猫脸任务保持一致
    }
        
    free_camera_flag = 1;
    vTaskDelete(NULL);
}


extern "C" __attribute__((weak)) void cat_detect_task(void* arg) {
    // 初始化猫脸检测器（优化参数：平衡检测率和误检率）
    static CatFaceDetectMN03 detectorCat(0.3F, 0.25F, 5, 0.3F);
    // 初始化AI数据
    init_ai_data(&g_ai_data);

    camera_fb_t *frame = NULL;
    dl::tool::Latency latency;
    while (1) {
        if (free_ai_flag == 1) {
            break;
        }
        if(xQueueReceive(camera_queue, &frame, pdMS_TO_TICKS(100)) == pdPASS) {
            // 基本帧验证
            if (!frame || !frame->buf || frame->width <= 0 || frame->height <= 0) {
                if (frame) {
                    esp_camera_fb_return(frame);
                }
                continue;
            }
            
            try {
                // 校验帧长度是否满足 RGB565 (2 bytes per pixel)
                size_t required_size = (size_t)frame->width * (size_t)frame->height * 2;
                if ((size_t)frame->len < required_size) {
                    esp_camera_fb_return(frame);
                    g_ai_data.cat_flag = false;
                    ai_push_result(&g_ai_data);
                    continue;
                }

                // 准备对齐的工作缓冲区，避免直接使用摄像头DMA帧参与推理
                if (!init_aligned_buffer(required_size)) {
                    esp_camera_fb_return(frame);
                    g_ai_data.cat_flag = false;
                    ai_push_result(&g_ai_data);
                    continue;
                }
                memcpy(g_aligned_buffer, frame->buf, required_size);

                latency.start();
                std::list<dl::detect::result_t> &detect_candidates = detectorCat.infer((uint16_t*)g_aligned_buffer, {(int)frame->height, (int)frame->width, 3});
                latency.end();
                
                // 处理检测结果
                if (detect_candidates.size() > 0) {
                    g_ai_data.cat_flag = true;
                    try {
                        draw_detection_result((uint16_t*)frame->buf, (int)frame->height, (int)frame->width, detect_candidates);
                    } catch (...) {
                        // 绘制异常忽略，保持流程继续
                    }
                    
                    // 提取第一个检测结果的信息
                    auto first_result = detect_candidates.begin();
                    if (first_result != detect_candidates.end()) {
                        g_ai_data.cat_detect.cat_frame_length = (int)first_result->box[2] - (int)first_result->box[0];
                        g_ai_data.cat_detect.cat_frame_width = (int)first_result->box[3] - (int)first_result->box[1];
                    }
                } else {
                    g_ai_data.cat_flag = false;
                }
                ai_push_result(&g_ai_data);
                camera_push_result(frame);
                
            } catch (...) {
                // 异常处理：释放帧缓冲区并重置状态
                if (frame) {
                    esp_camera_fb_return(frame);
                }
                g_ai_data.cat_flag = false;
                ai_push_result(&g_ai_data);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    cleanup_ai_resources();
    free_camera_flag = 1;
    vTaskDelete(NULL);
}

extern "C" __attribute__((weak)) void code_scanner_task(void* arg) {
    // 初始化AI数据
    init_ai_data(&g_ai_data);

    camera_fb_t *frame = NULL;
    // 扫描器创建一次复用，避免每帧创建/销毁导致碎片与开销
    static esp_image_scanner_t *esp_scn = NULL;
    while (1) {
        if (free_ai_flag == 1) {
            break;
        }
        if(xQueueReceive(camera_queue, &frame, pdMS_TO_TICKS(100)) == pdPASS) {
            // 基本帧验证
            if (!frame || !frame->buf || frame->width <= 0 || frame->height <= 0) {
                if (frame) {
                    esp_camera_fb_return(frame);
                }
                continue;
            }
            
            try {
                // 每帧创建与销毁，保留“复制数据”以避免悬空指针
                esp_image_scanner_t *esp_scn_local = esp_code_scanner_create();
                if (!esp_scn_local) {
                    esp_camera_fb_return(frame);
                    vTaskDelay(pdMS_TO_TICKS(50));
                    continue;
                }

                esp_code_scanner_config_t config = {ESP_CODE_SCANNER_MODE_FAST, ESP_CODE_SCANNER_IMAGE_RGB565, frame->width, frame->height};
                esp_code_scanner_set_config(esp_scn_local, config);

                int decoded_num = esp_code_scanner_scan_image(esp_scn_local, (uint8_t *)frame->buf);
                if (decoded_num) {
                    esp_code_scanner_symbol_t result = esp_code_scanner_result(esp_scn_local);
                    // 复制结果到自管缓冲，避免悬空指针
                    if (g_ai_data.code_data) {
                        free((void*)g_ai_data.code_data);
                        g_ai_data.code_data = NULL;
                    }
                    if (result.data) {
                        size_t len = strlen((const char*)result.data);
                        char *buf = (char*)malloc(len + 1);
                        if (buf) {
                            memcpy(buf, result.data, len);
                            buf[len] = '\0';
                            g_ai_data.code_data = buf;
                            g_ai_data.code_flag = true;
                        } else {
                            g_ai_data.code_data = NULL;
                            g_ai_data.code_flag = false;
                        }
                    } else {
                        g_ai_data.code_data = NULL;
                        g_ai_data.code_flag = false;
                    }
                } else {
                    // 未解码则清理旧数据，保持状态一致
                    if (g_ai_data.code_data) {
                        free((void*)g_ai_data.code_data);
                        g_ai_data.code_data = NULL;
                    }
                    g_ai_data.code_flag = false;
                }

                // 每帧完成后销毁本地扫描器
                esp_code_scanner_destroy(esp_scn_local);
                ai_push_result(&g_ai_data);
                camera_push_result(frame);

            } catch (...) {
                // 异常处理：释放帧缓冲区并重置状态
                if (frame) {
                    esp_camera_fb_return(frame);
                }
                g_ai_data.code_data = NULL;
                g_ai_data.code_flag = false;
                ai_push_result(&g_ai_data);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    cleanup_ai_resources();
    free_camera_flag = 1;
    vTaskDelete(NULL);
}


extern "C" __attribute__((weak)) void move_detect_task(void* arg) {
    // 初始化AI数据
    init_ai_data(&g_ai_data);

    camera_fb_t *frame = NULL;
    camera_fb_t *frame_last = NULL;
    
    while (1) {
        if (free_ai_flag == 1) {
            break;
        }
        
        if(xQueueReceive(camera_queue, &frame, pdMS_TO_TICKS(100)) == pdPASS) {
            
            // 获取第二帧进行比较
            if (xQueueReceive(camera_queue, &frame_last, portMAX_DELAY)) {
            
                    
                    uint32_t moving_point_number = dl::image::get_moving_point_number((uint16_t *)frame->buf, (uint16_t *)frame_last->buf, frame->height, frame->width, 8, 15);
                    if (moving_point_number > 10) {
                        g_ai_data.move_flag = true;
                    } else {
                        g_ai_data.move_flag = false;
                    }
                    
                    // 释放第一帧，推送第二帧
                    esp_camera_fb_return(frame);
                    camera_push_result(frame_last);
                    ai_push_result(&g_ai_data);
                    
            } else {
                esp_camera_fb_return(frame);
                g_ai_data.move_flag = false;
                ai_push_result(&g_ai_data);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    free_camera_flag = 1;
    vTaskDelete(NULL);
}

