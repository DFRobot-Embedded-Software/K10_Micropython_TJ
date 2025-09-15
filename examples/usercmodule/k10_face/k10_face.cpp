// 在包含任何头文件之前，先处理 ESP-IDF 宏问题

// 定义常量
#ifndef I2C_MASTER_NUM
#define I2C_MASTER_NUM 0
#endif

#ifndef I2C_MASTER_SDA_IO
#define I2C_MASTER_SDA_IO 47
#endif

#ifndef I2C_MASTER_SCL_IO
#define I2C_MASTER_SCL_IO 48
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
    
    // 分配新的对齐缓冲区
    g_aligned_buffer = (uint8_t*)heap_caps_aligned_alloc(16, size, MALLOC_CAP_8BIT | MALLOC_CAP_32BIT | MALLOC_CAP_DMA);
    if (g_aligned_buffer) {
        g_buffer_size = size;
        g_buffer_initialized = true;
        return true;
    }
    
    return false;
}

// 验证内存地址是否有效（宽松版本）
bool is_valid_memory_address(void* ptr, size_t size) {
    if (!ptr) return false;
    
    uintptr_t addr = (uintptr_t)ptr;
    
    // 检查地址是否在合理范围内（更宽松的范围）
    if (addr < 0x3F000000 || addr > 0x40000000) {
        return false;
    }
    
    // 检查地址是否对齐（只检查基本对齐）
    if (addr % 2 != 0) {
        return false;
    }
    
    // 不进行内存读取测试，避免触发异常
    return true;
}

// 简化的AI推理函数（带输入字节数，避免越界拷贝）
bool safe_ai_inference_with_protection(uint16_t* input_buf, int height, int width, size_t input_bytes,
    std::list<dl::detect::result_t>& results, CatFaceDetectMN03& detector) {
    // 基本检查
    if (!input_buf || height <= 0 || width <= 0) {
        return false;
    }
    
    // 计算所需内存大小（RGB565：每像素2字节）
    size_t required_size = (size_t)height * (size_t)width * 2;
    
    // 初始化对齐缓冲区
    if (!init_aligned_buffer(required_size)) {
        return false;
    }
    
    // 复制数据到对齐缓冲区（按最小值，避免越界读取）
    size_t copy_size = input_bytes < required_size ? input_bytes : required_size;
    memcpy(g_aligned_buffer, input_buf, copy_size);
    
    // 执行AI推理
    try {
        // 使用关键代码段保护
        taskENTER_CRITICAL(&ai_critical_mutex);
        
        // 使用对齐的缓冲区进行推理
        results = detector.infer((uint16_t*)g_aligned_buffer, {height, width, 3});
        
        taskEXIT_CRITICAL(&ai_critical_mutex);
        return true;
    } catch (const std::exception& e) {
        // 异常处理
        taskEXIT_CRITICAL(&ai_critical_mutex);
        return false;
    } catch (...) {
        // 未知异常
        taskEXIT_CRITICAL(&ai_critical_mutex);
        return false;
    }
    
    return false;
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

// AI推理保护函数
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
        
        // 临时提高当前任务优先级
        UBaseType_t current_priority = uxTaskPriorityGet(NULL);
        vTaskPrioritySet(NULL, configMAX_PRIORITIES - 1);
        
        // 禁用中断
        taskENTER_CRITICAL(&ai_critical_mutex);
        
        return true;
    }
    return false;
}

void exit_ai_inference_critical_section() {
    if (ai_inference_in_progress) {
        // 恢复中断
        taskEXIT_CRITICAL(&ai_critical_mutex);
        
        // 恢复任务优先级
        vTaskPrioritySet(NULL, uxTaskPriorityGet(NULL));
        
        ai_inference_in_progress = false;
        xSemaphoreGive(ai_inference_mutex);
    }
}

// 带性能监控的AI推理保护函数
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
        
        // 临时提高当前任务优先级
        UBaseType_t current_priority = uxTaskPriorityGet(NULL);
        vTaskPrioritySet(NULL, configMAX_PRIORITIES - 1);
        
        // 禁用中断
        taskENTER_CRITICAL(&ai_critical_mutex);
        
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
        
        // 恢复中断
        taskEXIT_CRITICAL(&ai_critical_mutex);
        
        // 恢复任务优先级
        vTaskPrioritySet(NULL, uxTaskPriorityGet(NULL));
        
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
            // 检查帧是否有效
            if (!frame || !frame->buf) {
                if (frame) {
                    esp_camera_fb_return(frame);
                }
                continue;
            }
            
            // 检查帧尺寸是否合理
            if (frame->width <= 0 || frame->height <= 0 || frame->len <= 0) {
                esp_camera_fb_return(frame);
                continue;
            }
            
            try {
                // 内存安全检查
                if (!frame->buf || frame->len == 0) {
                    esp_camera_fb_return(frame);
                    continue;
                }
                
                // 检查内存对齐
                if ((uintptr_t)frame->buf % 4 != 0) {
                    // 帧缓冲区未对齐，可能导致崩溃
                }
                
                // 检查堆栈使用情况
                UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
                if (stackHighWaterMark < 1024) {
                    // 堆栈空间不足
                }
                
                // 使用更安全的内存分配方式
                uint16_t *aligned_buf = NULL;
                bool need_free = false;
                
                // 如果缓冲区未对齐，创建对齐的副本
                if ((uintptr_t)frame->buf % 4 != 0) {
                    size_t aligned_size = frame->len + 4;
                    aligned_buf = (uint16_t*)heap_caps_aligned_alloc(4, aligned_size, MALLOC_CAP_8BIT | MALLOC_CAP_32BIT);
                    if (aligned_buf) {
                        memcpy(aligned_buf, frame->buf, frame->len);
                        need_free = true;
                    } else {
                        // 无法分配对齐内存
                        esp_camera_fb_return(frame);
                        continue;
                    }
                } else {
                    aligned_buf = (uint16_t*)frame->buf;
                }
                
                // 使用高级保护机制进入AI推理关键代码段
                if (enter_ai_inference_critical_section()) {
                    // 实际的人脸检测逻辑（避免持有引用，使用拷贝以防悬挂引用）
                    std::list<dl::detect::result_t> detect_candidates = detectorFace.infer(aligned_buf, {(int)frame->height, (int)frame->width, 3});
                    std::list<dl::detect::result_t> detect_results = detectorFace2.infer(aligned_buf, {(int)frame->height, (int)frame->width, 3}, detect_candidates);
                    
                    // 退出AI推理关键代码段
                    exit_ai_inference_critical_section();
                    
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
                        draw_detection_result(aligned_buf, (int)frame->height, (int)frame->width, detect_results);
                    } else {
                        g_ai_data.face_flag = false;
                        g_ai_data.face_detect.face_id = -1;
                    }
                } else {
                    // 如果无法进入关键代码段，设置默认值
                    g_ai_data.face_flag = false;
                    g_ai_data.face_detect.face_id = -1;
                }
                
                // 释放对齐的内存副本
                if (need_free && aligned_buf) {
                    heap_caps_free(aligned_buf);
                }
            
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
            // 检查帧是否有效
            if (!frame || !frame->buf) {
                if (frame) {
                    esp_camera_fb_return(frame);
                }
                continue;
            }
            
            // 检查帧尺寸是否合理
            if (frame->width <= 0 || frame->height <= 0 || frame->len <= 0) {
                esp_camera_fb_return(frame);
                continue;
            }
            
            try {
                // 内存安全检查
                if (!frame->buf || frame->len == 0) {
                    esp_camera_fb_return(frame);
                    continue;
                }
                
                // 检查内存对齐
                if ((uintptr_t)frame->buf % 4 != 0) {
                    // 帧缓冲区未对齐，可能导致崩溃
                }
                
                // 检查堆栈使用情况
                UBaseType_t stackHighWaterMark = uxTaskGetStackHighWaterMark(NULL);
                if (stackHighWaterMark < 1024) {
                    // 堆栈空间不足
                }
                
                // 使用更安全的方式执行AI推理
                std::list<dl::detect::result_t> detect_candidates;
                bool inference_success = false;
                
                // 使用新的安全AI推理函数
                inference_success = safe_ai_inference_with_protection(
                    (uint16_t*)frame->buf, 
                    (int)frame->height, 
                    (int)frame->width,
                    (size_t)frame->len,
                    detect_candidates, 
                    detectorCat
                );
                
                // 如果安全函数失败，尝试直接推理
                if (!inference_success) {
                    try {
                        taskENTER_CRITICAL(&ai_critical_mutex);
                        detect_candidates = detectorCat.infer((uint16_t*)frame->buf, {(int)frame->height, (int)frame->width, 3});
                        taskEXIT_CRITICAL(&ai_critical_mutex);
                        inference_success = true;
                    } catch (const std::exception& e) {
                        taskEXIT_CRITICAL(&ai_critical_mutex);
                        inference_success = false;
                    } catch (...) {
                        taskEXIT_CRITICAL(&ai_critical_mutex);
                        inference_success = false;
                    }
                }
                
                // 处理推理结果
                if (inference_success) {
                    if (detect_candidates.size() > 0) {
                        g_ai_data.cat_flag = true;
                        draw_detection_result((uint16_t*)frame->buf, (int)frame->height, (int)frame->width, detect_candidates);
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
            // 检查帧是否有效
            if (!frame || !frame->buf) {
                if (frame) {
                    esp_camera_fb_return(frame);
                }
                continue;
            }
            
            // 检查帧尺寸是否合理
            if (frame->width <= 0 || frame->height <= 0 || frame->len <= 0) {
                esp_camera_fb_return(frame);
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
            // 检查第一帧是否有效
            if (!frame || !frame->buf) {
                if (frame) {
                    esp_camera_fb_return(frame);
                }
                continue;
            }
            
            // 检查第一帧尺寸是否合理
            if (frame->width <= 0 || frame->height <= 0 || frame->len <= 0) {
                esp_camera_fb_return(frame);
                continue;
            }
            
            // 获取第二帧进行比较
            if (xQueueReceive(camera_queue, &frame_last, portMAX_DELAY)) {
                // 检查第二帧是否有效
                if (!frame_last || !frame_last->buf) {
                    esp_camera_fb_return(frame);
                    if (frame_last) {
                        esp_camera_fb_return(frame_last);
                    }
                    continue;
                }
                
                // 检查第二帧尺寸是否合理
                if (frame_last->width <= 0 || frame_last->height <= 0 || frame_last->len <= 0) {
                    esp_camera_fb_return(frame);
                    esp_camera_fb_return(frame_last);
                    continue;
                }
                
                try {
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

