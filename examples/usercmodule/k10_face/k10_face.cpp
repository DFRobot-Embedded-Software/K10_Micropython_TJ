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

// 验证内存地址是否有效（更严格的版本）
bool is_valid_memory_address(void* ptr, size_t size) {
    if (!ptr) return false;
    
    uintptr_t addr = (uintptr_t)ptr;
    
    // 检查地址是否在合理范围内（ESP32内存映射）
    if (addr < 0x3F000000 || addr > 0x50000000) {
        return false;
    }
    
    // 检查地址是否对齐（2字节对齐即可）
    if (addr % 2 != 0) {
        return false;
    }
    
    // 检查地址是否在有效内存区域
    if (addr >= 0x40000000 && addr < 0x50000000) {
        // 这是PSRAM区域，需要额外检查
        return true;
    }
    
    // 检查是否在内部RAM区域
    if (addr >= 0x3F000000 && addr < 0x40000000) {
        return true;
    }
    
    return false;
}

// 验证帧缓冲区是否安全可用（宽松版本用于调试）
bool is_safe_frame_buffer(camera_fb_t* frame) {
    if (!frame) return false;
    
    // 检查基本字段
    if (frame->width <= 0 || frame->height <= 0 || frame->len <= 0) {
        return false;
    }
    
    // 检查缓冲区指针
    if (!frame->buf) return false;
    
    // 检查尺寸是否在合理范围内
    if (frame->width > 4096 || frame->height > 4096) {
        return false;
    }
    
    // 检查缓冲区大小是否合理（RGB565格式：每像素2字节）
    size_t expected_size = (size_t)frame->width * (size_t)frame->height * 2;
    if (frame->len < expected_size) {
        return false;
    }
    
    // 暂时放宽内存地址验证，只检查基本有效性
    uintptr_t addr = (uintptr_t)frame->buf;
    if (addr == 0 || addr < 0x10000000 || addr > 0x60000000) {
        return false;
    }
    
    return true;
}

// 超安全的内存访问函数，避免任何可能导致崩溃的操作
bool ultra_safe_memory_check(void* ptr, size_t size) {
    if (!ptr || size == 0) return false;
    
    uintptr_t addr = (uintptr_t)ptr;
    
    // 基本范围检查
    if (addr < 0x10000000 || addr > 0x60000000) {
        return false;
    }
    
    // 检查地址是否合理对齐
    if (addr % 2 != 0) {
        return false;
    }
    
    // 不进行实际的内存读取测试，避免触发异常
    return true;
}

// 简单的基于颜色的检测函数（替代AI推理）
bool simple_color_based_detection(uint16_t* image_buf, int height, int width) {
    if (!image_buf || height <= 0 || width <= 0) {
        return false;
    }
    
    // 简单的颜色统计检测
    int total_pixels = height * width;
    int skin_tone_pixels = 0;
    
    // 只检查中心区域，避免边界问题
    int start_x = width / 4;
    int end_x = width * 3 / 4;
    int start_y = height / 4;
    int end_y = height * 3 / 4;
    
    for (int y = start_y; y < end_y; y++) {
        for (int x = start_x; x < end_x; x++) {
            uint16_t pixel = image_buf[y * width + x];
            
            // 提取RGB565颜色分量
            int r = (pixel >> 11) & 0x1F;
            int g = (pixel >> 5) & 0x3F;
            int b = pixel & 0x1F;
            
            // 简单的肤色检测（R > G > B）
            if (r > g && g > b && r > 15) {
                skin_tone_pixels++;
            }
        }
    }
    
    // 如果肤色像素超过总像素的5%，认为检测到人脸
    return (skin_tone_pixels * 100 / total_pixels) > 5;
}

// 安全的帧缓冲区访问包装器（简化版本，不使用std::function）
bool safe_access_frame_buffer(camera_fb_t* frame) {
    if (!is_safe_frame_buffer(frame)) {
        return false;
    }
    
    // 简化的内存地址验证，不使用关键代码段
    return ultra_safe_memory_check(frame->buf, frame->len);
}

// 超安全的AI推理函数（完全避免内存拷贝和复杂操作）
bool ultra_safe_ai_inference(uint16_t* input_buf, int height, int width, size_t input_bytes,
    std::list<dl::detect::result_t>& results, CatFaceDetectMN03& detector) {
    // 基本检查
    if (!input_buf || height <= 0 || width <= 0) {
        return false;
    }
    
    // 计算所需内存大小（RGB565：每像素2字节）
    size_t required_size = (size_t)height * (size_t)width * 2;
    
    // 长度必须满足完整帧
    if (input_bytes < required_size) {
        return false;
    }
    
    // 额外的内存安全检查
    if (!ultra_safe_memory_check(input_buf, required_size)) {
        return false;
    }
    
    // 添加内存屏障，确保内存操作完成
    __asm__ __volatile__("" ::: "memory");
    
    // 直接使用输入缓冲区，不进行内存拷贝
    // 这避免了内存拷贝可能导致的问题
    try {
        results = detector.infer(input_buf, {height, width, 3});
        return true;
    } catch (const std::exception& e) {
        // 异常处理
        results.clear();
        return false;
    } catch (...) {
        // 未知异常
        results.clear();
        return false;
    }
}

// 清理内存资源
void cleanup_ai_resources() {
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
}

extern "C" int register_face_flag;
extern "C" int recognize_face_flag;
extern "C" int remove_face_flag;
extern "C" int reset_faces_flag;
extern "C" int free_camera_flag;
extern "C" int free_ai_flag;
//extern "C" QueueHandle_t camera_queue;


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

// AI推理保护函数（简化版本，避免复杂的优先级操作）
bool enter_ai_inference_critical_section() {
    if (ai_inference_mutex == NULL) {
        ai_inference_mutex = xSemaphoreCreateMutex();
        if (ai_inference_mutex == NULL) {
            return false;
        }
    }
    
    // 尝试获取互斥锁，最多等待100ms
    if (xSemaphoreTake(ai_inference_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ai_inference_in_progress = true;
        return true;
    }
    return false;
}

void exit_ai_inference_critical_section() {
    if (ai_inference_in_progress) {
        ai_inference_in_progress = false;
        xSemaphoreGive(ai_inference_mutex);
    }
}

// 带性能监控的AI推理保护函数（简化版本）
bool enter_ai_inference_critical_section_with_timing(uint32_t *start_time) {
    if (ai_inference_mutex == NULL) {
        ai_inference_mutex = xSemaphoreCreateMutex();
        if (ai_inference_mutex == NULL) {
            return false;
        }
    }
    
    // 尝试获取互斥锁，最多等待100ms
    if (xSemaphoreTake(ai_inference_mutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        ai_inference_in_progress = true;
        
        // 记录开始时间
        *start_time = esp_timer_get_time() / 1000; // 转换为毫秒
        
        return true;
    }
    return false;
}

void exit_ai_inference_critical_section_with_timing(uint32_t start_time) {
    if (ai_inference_in_progress) {
        // 计算执行时间
        uint32_t end_time = esp_timer_get_time() / 1000; // 转换为毫秒
        uint32_t execution_time = end_time - start_time;
        
        // 更新性能统计
        ai_inference_count++;
        ai_inference_total_time += execution_time;
        if (execution_time > ai_inference_max_time) {
            ai_inference_max_time = execution_time;
        }
        
        // 每100次推理输出一次统计信息
        if (ai_inference_count % 100 == 0) {
            uint32_t avg_time = ai_inference_total_time / ai_inference_count;
            // AI推理统计 - 次数、平均时间、最大时间
        }
        
        ai_inference_in_progress = false;
        xSemaphoreGive(ai_inference_mutex);
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


static ai_data_obj_t g_ai_data;

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
    esp_err_t err = esp_camera_deinit();
    vTaskDelete(NULL);
}


extern "C" __attribute__((weak))  void face_recognize_start_task(void* arg) {

    
    HumanFaceDetectMSR01 detectorFace(0.3F, 0.3F, 10, 0.3F);
    HumanFaceDetectMNP01 detectorFace2(0.4F, 0.3F, 10);
    FaceRecognition112V1S16  *recognizer = new FaceRecognition112V1S16();
    recognizer->set_partition(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "fr");
    recognizer->set_ids_from_flash();
    // 初始化AI数据
    init_ai_data(&g_ai_data);

    camera_fb_t *frame = NULL;

    dl::tool::Latency latency;

    while (1) {
        if (free_ai_flag == 1) {
            vTaskDelay(pdMS_TO_TICKS(100)); // 等待100ms后重试
            break;
        }
        // 检查camera_queue是否已初始化
        if (!camera_queue) {
            vTaskDelay(pdMS_TO_TICKS(100)); // 等待100ms后重试
            continue;
        }
        
        if(xQueueReceive(camera_queue, &frame, portMAX_DELAY) == pdPASS) {
            // 使用新的安全验证函数检查帧
            if (!is_safe_frame_buffer(frame)) {
                if (frame) {
                    // 记录被拒绝的帧信息用于调试
                    // printf("Face task: Frame rejected - width=%d, height=%d, len=%d, buf=%p\n", 
                    //        frame->width, frame->height, frame->len, frame->buf);
                    esp_camera_fb_return(frame);
                }
                continue;
            }
            
            try {
                // 检查堆栈使用情况
                UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
                if (stackHighWaterMark < 1024) {
                    // 堆栈空间不足，释放帧并继续
                    esp_camera_fb_return(frame);
                    continue;
                }
                
                // 由于已经通过is_safe_frame_buffer验证，直接使用frame->buf
                uint16_t *aligned_buf = (uint16_t*)frame->buf;
                
                // 使用高级保护机制进入AI推理关键代码段
                if (enter_ai_inference_critical_section()) {
                    // 使用超安全的内存检查
                    if (!ultra_safe_memory_check(aligned_buf, frame->len)) {
                        exit_ai_inference_critical_section();
                        esp_camera_fb_return(frame);
                        continue;
                    }
                    
                    // 恢复AI推理功能，使用新的ESP-DL版本
                    latency.start();
                    std::list<dl::detect::result_t> detect_candidates;
                    std::list<dl::detect::result_t> detect_results;
                    latency.end();
                    // 使用AI推理进行人脸检测
                    try {
                        if (ultra_safe_memory_check(aligned_buf, frame->len)) {
                            detect_candidates = detectorFace.infer(aligned_buf, {(int)frame->height, (int)frame->width, 3});
                            detect_results = detectorFace2.infer(aligned_buf, {(int)frame->height, (int)frame->width, 3}, detect_candidates);
                        } else {
                            // 内存验证失败，设置默认值
                            detect_candidates.clear();
                            detect_results.clear();
                        }
                    } catch (const std::exception& e) {
                        // AI推理异常，设置默认值
                        detect_candidates.clear();
                        detect_results.clear();
                    } catch (...) {
                        // 未知异常
                        detect_candidates.clear();
                        detect_results.clear();
                    }
                    
                    // 退出AI推理关键代码段
                    exit_ai_inference_critical_section();
                    
                    // 使用AI推理的结果
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
                                recognizer->enroll_id(aligned_buf, {(int)frame->height, (int)frame->width, 3}, detect_results.front().keypoint, "", true);
                            }
                            register_face_flag = 0;
                        }

                        // 人脸识别
                        if(recognize_face_flag == 1){
                            if (!detect_results.empty() && !detect_results.front().keypoint.empty()) {
                                recognize_result = recognizer->recognize(aligned_buf, {(int)frame->height, (int)frame->width, 3}, detect_results.front().keypoint);
                                g_ai_data.face_detect.face_id = recognize_result.id;
                            }
                            recognize_face_flag = 0;
                        }
                        
                        // 画检测框
                        try {
                            draw_detection_result(aligned_buf, (int)frame->height, (int)frame->width, detect_results);
                        } catch (...) {
                            // 绘制阶段异常保护
                        }
                    } else {
                        g_ai_data.face_flag = false;
                        g_ai_data.face_detect.face_id = -1;
                    }
                } else {
                    // 如果无法进入关键代码段，设置默认值
                    g_ai_data.face_flag = false;
                    g_ai_data.face_detect.face_id = -1;
                }
                
                // 由于直接使用frame->buf，无需释放额外内存
            
                // 推送结果到队列
                ai_push_result(&g_ai_data);
                camera_push_result(frame);
                
            } catch (const std::exception& e) {
                // 处理异常，确保释放帧缓冲区
                if (frame) {
                    esp_camera_fb_return(frame);
                }
                g_ai_data.face_flag = false;
                g_ai_data.face_detect.face_id = -1;
                ai_push_result(&g_ai_data);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1)); // 增加延时，避免过度占用CPU
    }
    free_camera_flag = 1;
    vTaskDelete(NULL);
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

extern "C" __attribute__((weak)) void cat_detect_task(void* arg) {
    static CatFaceDetectMN03 detectorCat(0.4F, 0.3F, 10, 0.3F);
    // 初始化AI数据
    init_ai_data(&g_ai_data);

    camera_fb_t *frame = NULL;
    dl::tool::Latency latency;
    while (1) {
        if (free_ai_flag == 1) {
            vTaskDelay(pdMS_TO_TICKS(100)); // 等待100ms后重试
            break;
        }
        // 检查camera_queue是否已初始化
        if (!camera_queue) {
            vTaskDelay(pdMS_TO_TICKS(100)); // 等待100ms后重试
            continue;
        }
        
        if(xQueueReceive(camera_queue, &frame, portMAX_DELAY) == pdPASS) {
            // 使用新的安全验证函数检查帧
            if (!is_safe_frame_buffer(frame)) {
                if (frame) {
                    esp_camera_fb_return(frame);
                }
                continue;
            }
            
            try {
                // 检查堆栈使用情况
                UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
                if (stackHighWaterMark < 1024) {
                    // 堆栈空间不足，释放帧并继续
                    esp_camera_fb_return(frame);
                    continue;
                }
                
                // 使用更安全的方式执行AI推理
                latency.start();
                std::list<dl::detect::result_t> detect_candidates;
                latency.end();
                bool inference_success = false;
                
                // 使用超安全的内存检查
                if (!ultra_safe_memory_check(frame->buf, frame->len)) {
                    esp_camera_fb_return(frame);
                    continue;
                }
                
                // 恢复AI推理，使用新的ESP-DL版本
                inference_success = ultra_safe_ai_inference(
                    (uint16_t*)frame->buf, 
                    (int)frame->height, 
                    (int)frame->width,
                    (size_t)frame->len,
                    detect_candidates, 
                    detectorCat
                );
                
                // 如果超安全函数失败，不再尝试备用方案，直接设置失败
                // 这避免了任何可能导致内存访问问题的操作
                
                // 处理推理结果
                if (inference_success) {
                    if (detect_candidates.size() > 0) {
                        g_ai_data.cat_flag = true;
                        try {
                            draw_detection_result((uint16_t*)frame->buf, (int)frame->height, (int)frame->width, detect_candidates);
                        } catch (...) {
                            // 绘制阶段异常保护
                        }
                        std::list<dl::detect::result_t>::iterator first_result = detect_candidates.begin();
                        if (first_result != detect_candidates.end()) {
                            g_ai_data.cat_detect.cat_frame_length = (int)first_result->box[2] - (int)first_result->box[0];
                            g_ai_data.cat_detect.cat_frame_width = (int)first_result->box[3] - (int)first_result->box[1];
                        }
                    } else {
                        g_ai_data.cat_flag = false;
                    }
                } else {
                    // 如果AI推理失败，设置默认值
                    g_ai_data.cat_flag = false;
                }
                ai_push_result(&g_ai_data);
                camera_push_result(frame);
                
            } catch (const std::exception& e) {
                // 处理异常，确保释放帧缓冲区
                if (frame) {
                    esp_camera_fb_return(frame);
                }
                g_ai_data.cat_flag = false;
                ai_push_result(&g_ai_data);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
        // xl9555_write_ai(0x03, 0x00);
        // vTaskDelay(pdMS_TO_TICKS(100));
        // xl9555_write_ai(0x03, 0x80);
    }
    cleanup_ai_resources();
    free_camera_flag = 1;
    vTaskDelete(NULL);
}

extern "C" __attribute__((weak)) void code_scanner_task(void* arg) {
    // 初始化AI数据
    init_ai_data(&g_ai_data);

    camera_fb_t *frame = NULL;
    while (1) {
        if (free_ai_flag == 1) {
            vTaskDelay(pdMS_TO_TICKS(100)); // 等待100ms后重试
            break;
        }
        // 检查camera_queue是否已初始化
        if (!camera_queue) {
            vTaskDelay(pdMS_TO_TICKS(100)); // 等待100ms后重试
            continue;
        }
        
        if(xQueueReceive(camera_queue, &frame, portMAX_DELAY) == pdPASS) {
            // 使用新的安全验证函数检查帧
            if (!is_safe_frame_buffer(frame)) {
                if (frame) {
                    esp_camera_fb_return(frame);
                }
                continue;
            }
            
            try {
                esp_image_scanner_t *esp_scn = esp_code_scanner_create();
                if (!esp_scn) {
                    esp_camera_fb_return(frame);
                    continue;
                }
                
                esp_code_scanner_config_t config = {ESP_CODE_SCANNER_MODE_FAST, ESP_CODE_SCANNER_IMAGE_RGB565, frame->width, frame->height};
                esp_code_scanner_set_config(esp_scn, config);
                int decoded_num = esp_code_scanner_scan_image(esp_scn, (uint8_t *)frame->buf);
                if(decoded_num){
                    esp_code_scanner_symbol_t result = esp_code_scanner_result(esp_scn);
                    g_ai_data.code_data = result.data;
                    g_ai_data.code_flag = true;
                }else{
                    g_ai_data.code_data = NULL;
                    g_ai_data.code_flag = false;
                }
                esp_code_scanner_destroy(esp_scn);
                ai_push_result(&g_ai_data);
                camera_push_result(frame);
                
            } catch (const std::exception& e) {
                // 处理异常，确保释放帧缓冲区
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
            vTaskDelay(pdMS_TO_TICKS(100)); // 等待100ms后重试
            break;
        }
        // 检查camera_queue是否已初始化
        if (!camera_queue) {
            vTaskDelay(pdMS_TO_TICKS(100)); // 等待100ms后重试
            continue;
        }
        
        if(xQueueReceive(camera_queue, &frame, portMAX_DELAY) == pdPASS) {
            // 使用新的安全验证函数检查第一帧
            if (!is_safe_frame_buffer(frame)) {
                if (frame) {
                    esp_camera_fb_return(frame);
                }
                continue;
            }
            
            // 获取第二帧进行比较
            if (xQueueReceive(camera_queue, &frame_last, portMAX_DELAY)) {
                // 使用新的安全验证函数检查第二帧
                if (!is_safe_frame_buffer(frame_last)) {
                    esp_camera_fb_return(frame);
                    if (frame_last) {
                        esp_camera_fb_return(frame_last);
                    }
                    continue;
                }
                
                try {
                    // 使用超安全的内存检查
                    if (!ultra_safe_memory_check(frame->buf, frame->len) || 
                        !ultra_safe_memory_check(frame_last->buf, frame_last->len)) {
                        esp_camera_fb_return(frame);
                        esp_camera_fb_return(frame_last);
                        continue;
                    }
                    
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
                    
                } catch (const std::exception& e) {
                    // 处理异常，确保释放帧缓冲区
                    esp_camera_fb_return(frame);
                    esp_camera_fb_return(frame_last);
                    g_ai_data.move_flag = false;
                    ai_push_result(&g_ai_data);
                }
            } else {
                // 如果无法获取第二帧，释放第一帧
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

