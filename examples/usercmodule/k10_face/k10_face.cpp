// 在包含任何头文件之前，先处理 ESP-IDF 宏问题


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
        dl::image::draw_hollow_rectangle(image_ptr, image_height, image_width,
                                         DL_MAX(prediction->box[0], 0),
                                         DL_MAX(prediction->box[1], 0),
                                         DL_MAX(prediction->box[2], 0),
                                         DL_MAX(prediction->box[3], 0),
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
        vTaskDelay(pdMS_TO_TICKS(1));
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
                // 实际的人脸检测逻辑
                std::list<dl::detect::result_t> &detect_candidates = detectorFace.infer((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3});
                std::list<dl::detect::result_t> &detect_results = detectorFace2.infer((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3}, detect_candidates);
                
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
                        recognizer->enroll_id((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3}, detect_results.front().keypoint, "", true);
                    }
                    register_face_flag = 0;
                }

                // 人脸识别
                if(recognize_face_flag == 1){
                    if (!detect_results.empty() && !detect_results.front().keypoint.empty()) {
                        recognize_result = recognizer->recognize((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3}, detect_results.front().keypoint);
                        g_ai_data.face_detect.face_id = recognize_result.id;
                    }
                    recognize_face_flag = 0;
                }
                
                // 画检测框
                draw_detection_result((uint16_t *)frame->buf, (int)frame->height, (int)frame->width, detect_results);
            } else {
                g_ai_data.face_flag = false;
                g_ai_data.face_detect.face_id = -1;
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
                std::list<dl::detect::result_t> &detect_candidates = detectorCat.infer((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3});
            if (detect_candidates.size() > 0) {
                g_ai_data.cat_flag = true;
                draw_detection_result((uint16_t *)frame->buf, (int)frame->height, (int)frame->width, detect_candidates);
                std::list<dl::detect::result_t>::iterator first_result = detect_candidates.begin();
                if (first_result != detect_candidates.end()) {
                    g_ai_data.cat_detect.cat_frame_length = (int)first_result->box[2] - (int)first_result->box[0];
                    g_ai_data.cat_detect.cat_frame_width = (int)first_result->box[3] - (int)first_result->box[1];
                }
            } else {
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

