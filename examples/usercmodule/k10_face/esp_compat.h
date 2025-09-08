#ifndef ESP_COMPAT_H
#define ESP_COMPAT_H

// 禁用可能导致冲突的宏
#ifdef ESP_STATIC_ASSERT
#undef ESP_STATIC_ASSERT
#endif

// 重新定义 ESP_STATIC_ASSERT 以避免冲突
#define ESP_STATIC_ASSERT(cond, msg) static_assert(cond, msg)

// 包含必要的 ESP-IDF 头文件
extern "C" {
    #include "esp_log.h"
    #include "esp_system.h"
    #include "esp_partition.h"
    #include "freertos/FreeRTOS.h"
    #include "freertos/semphr.h"
    #include "freertos/task.h"
    #include "freertos/queue.h"
    #include "esp_camera.h"
}

#endif // ESP_COMPAT_H
