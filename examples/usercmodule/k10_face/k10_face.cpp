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



static QueueHandle_t ai_camera_queue = NULL;
static face_info_t recognize_result;

extern "C" int register_face_flag;
extern "C" int recognize_face_flag;
extern "C" int remove_face_flag;
extern "C" int reset_faces_flag;

extern "C" void ai_push_result(ai_data_obj_t *data);

esp_err_t xl9555_write_ai(uint8_t reg, uint8_t data) {
    uint8_t buf[2] = {reg, data};
    return i2c_master_write_to_device(I2C_MASTER_NUM, XL9555_ADDR, buf, 2, 1000 / portTICK_PERIOD_MS);
}
esp_err_t xl9555_read_ai(uint8_t reg, uint8_t *data) {
    return i2c_master_write_read_device(I2C_MASTER_NUM, XL9555_ADDR, &reg, 1, data, 1, 1000 / portTICK_PERIOD_MS);
}


static ai_data_obj_t g_ai_data;

extern "C" __attribute__((weak)) void ai_camera_task(void* arg) {
    
    if (!ai_camera_queue) {
        ai_camera_queue = xQueueCreate(10, sizeof(camera_fb_t *)); // 最多缓存10个结果
    }
    
    while (1) {
        xl9555_write_ai(0x03, 0x80);
        camera_fb_t *frame = esp_camera_fb_get();
        if (frame){
            xQueueSend(ai_camera_queue, &frame, portMAX_DELAY);
        }
        vTaskDelay(pdMS_TO_TICKS(50));
        xl9555_write_ai(0x03, 0x00);
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}


extern "C" __attribute__((weak))  void ai_task(void* arg) {

    
    HumanFaceDetectMSR01 detectorFace(0.3F, 0.3F, 10, 0.3F);
    HumanFaceDetectMNP01 detectorFace2(0.4F, 0.3F, 10);
    FaceRecognition112V1S16  *recognizer = new FaceRecognition112V1S16();
    recognizer->set_partition(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, "fr");
    recognizer->set_ids_from_flash();
   
    
    // 初始化AI数据
    memset(&g_ai_data, 0, sizeof(g_ai_data));

    camera_fb_t *frame = NULL;

     

    while (1) {
        //mp_print_face_cstr("ai_task\n");
        if(xQueueReceive(ai_camera_queue, &frame, portMAX_DELAY) == pdPASS) {
            if (frame && frame->buf) {

            }
                // 实际的人脸检测逻辑
                std::list<dl::detect::result_t> &detect_candidates =  detectorFace.infer((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3});
                std::list<dl::detect::result_t> &detect_results = detectorFace2.infer((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3}, detect_candidates);
                
                if (detect_results.size() > 0) {
                    //mp_print_face_cstr("face detected\n");
                    g_ai_data.face_flag = true;
                    std::list<dl::detect::result_t>::iterator first_result = detect_results.begin();
                    if (first_result != detect_results.end()) {
                        g_ai_data.face_detect.face_frame_length = (int)first_result->box[2] - (int)first_result->box[0];
                        g_ai_data.face_detect.face_frame_width = (int)first_result->box[3] - (int)first_result->box[1];
                    }
                    
                    if(register_face_flag == 1){
                        recognizer->enroll_id((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3}, detect_results.front().keypoint, "",true);
                        register_face_flag = 0;
                    }

                    if(recognize_face_flag == 1){
                        recognize_result = recognizer->recognize((uint16_t *)frame->buf, {(int)frame->height, (int)frame->width, 3}, detect_results.front().keypoint);
                        //g_ai_data.face_detect.face_id = recognize_result.id;
                        //mp_print_face_cstr("Recognize face success\n");
                        recognize_face_flag = 0;
                    }
                    
                    
                } else {
                    //mp_print_face_cstr("no face detected\n");
                    g_ai_data.face_flag = false;
                    g_ai_data.face_detect.face_id = -1;
                }
                
                // 推送结果到队列
                ai_push_result(&g_ai_data);
                
                // 释放帧缓冲区
                esp_camera_fb_return(frame);
        }
        vTaskDelay(pdMS_TO_TICKS(100)); // 增加延时，避免过度占用CPU
    }
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
        //.master.clk_speed = I2C_MASTER_FREQ_HZ,
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
    xl9555_write_ai(0x06, 0xfb);
    xl9555_write_ai(0x02, 0x00);
    vTaskDelay(pdMS_TO_TICKS(100));
    xl9555_write_ai(0x02, 0x02);
    vTaskDelay(pdMS_TO_TICKS(100));
    //默认关闭用户灯
    xl9555_write_ai(0x03, 0x00);
}