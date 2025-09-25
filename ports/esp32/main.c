/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * Development of the code in this file was sponsored by Microbric Pty Ltd
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2016 Damien P. George
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <sys/time.h>
#include <time.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_task.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_memory_utils.h"
#include "esp_psram.h"

#include "py/cstack.h"
#include "py/nlr.h"
#include "py/compile.h"
#include "py/runtime.h"
#include "py/persistentcode.h"
#include "py/repl.h"
#include "py/gc.h"
#include "py/mphal.h"
#include "shared/readline/readline.h"
#include "shared/runtime/pyexec.h"
#include "shared/timeutils/timeutils.h"
#include "shared/tinyusb/mp_usbd.h"
#include "mbedtls/platform_time.h"

#include "uart.h"
#include "usb.h"
#include "usb_serial_jtag.h"
#include "modmachine.h"
#include "modnetwork.h"

#if MICROPY_BLUETOOTH_NIMBLE
#include "extmod/modbluetooth.h"
#endif

#if MICROPY_PY_ESPNOW
#include "modespnow.h"
#endif

// MicroPython runs as a task under FreeRTOS
#define MP_TASK_PRIORITY        (ESP_TASK_PRIO_MIN + 1)

typedef struct _native_code_node_t {
    struct _native_code_node_t *next;
    uint32_t data[];
} native_code_node_t;

static native_code_node_t *native_code_head = NULL;

static void esp_native_code_free_all(void);

int vprintf_null(const char *format, va_list ap) {
    // do nothing: this is used as a log target during raw repl mode
    return 0;
}

time_t platform_mbedtls_time(time_t *timer) {
    // mbedtls_time requires time in seconds from EPOCH 1970

    struct timeval tv;
    gettimeofday(&tv, NULL);

    return tv.tv_sec + TIMEUTILS_SECONDS_1970_TO_2000;
}

void mp_task(void *pvParameter) {
    volatile uint32_t sp = (uint32_t)esp_cpu_get_sp();
    #if MICROPY_PY_THREAD
    mp_thread_init(pxTaskGetStackStart(NULL), MICROPY_TASK_STACK_SIZE / sizeof(uintptr_t));
    #endif
    #if MICROPY_HW_ESP_USB_SERIAL_JTAG
    usb_serial_jtag_init();
    #elif MICROPY_HW_ENABLE_USBDEV
    usb_init();
    #endif
    #if MICROPY_HW_ENABLE_UART_REPL
    uart_stdout_init();
    #endif
    machine_init();

    // Configure time function, for mbedtls certificate time validation.
    mbedtls_platform_set_time(platform_mbedtls_time);

    esp_err_t err = esp_event_loop_create_default();
    if (err != ESP_OK) {
        ESP_LOGE("esp_init", "can't create event loop: 0x%x\n", err);
    }

    void *mp_task_heap = MP_PLAT_ALLOC_HEAP(MICROPY_GC_INITIAL_HEAP_SIZE);
    if (mp_task_heap == NULL) {
        printf("mp_task_heap allocation failed!\n");
        esp_restart();
    }

soft_reset:
    // initialise the stack pointer for the main thread
    mp_cstack_init_with_top((void *)sp, MICROPY_TASK_STACK_SIZE);
    gc_init(mp_task_heap, mp_task_heap + MICROPY_GC_INITIAL_HEAP_SIZE);
    mp_init();
    mp_obj_list_append(mp_sys_path, MP_OBJ_NEW_QSTR(MP_QSTR__slash_lib));
    readline_init0();

    // initialise peripherals
    machine_pins_init();
    #if MICROPY_PY_MACHINE_I2S
    machine_i2s_init0();
    #endif

    // run boot-up scripts
    pyexec_frozen_module("_boot.py", false);
    int ret = pyexec_file_if_exists("boot.py");
    if (ret & PYEXEC_FORCED_EXIT) {
        goto soft_reset_exit;
    }
    if (pyexec_mode_kind == PYEXEC_MODE_FRIENDLY_REPL && ret != 0) {
        int ret = pyexec_file_if_exists("main.py");
        if (ret & PYEXEC_FORCED_EXIT) {
            goto soft_reset_exit;
        }
    }

    for (;;) {
        if (pyexec_mode_kind == PYEXEC_MODE_RAW_REPL) {
            vprintf_like_t vprintf_log = esp_log_set_vprintf(vprintf_null);
            if (pyexec_raw_repl() != 0) {
                break;
            }
            esp_log_set_vprintf(vprintf_log);
        } else {
            if (pyexec_friendly_repl() != 0) {
                break;
            }
        }
        vTaskDelay(1);
    }

soft_reset_exit:

    #if MICROPY_BLUETOOTH_NIMBLE
    mp_bluetooth_deinit();
    #endif

    #if MICROPY_PY_ESPNOW
    espnow_deinit(mp_const_none);
    MP_STATE_PORT(espnow_singleton) = NULL;
    #endif

    machine_timer_deinit_all();

    #if MICROPY_PY_THREAD
    mp_thread_deinit();
    #endif

    #if MICROPY_HW_ENABLE_USB_RUNTIME_DEVICE
    mp_usbd_deinit();
    #endif

    gc_sweep_all();

    // Free any native code pointers that point to iRAM.
    esp_native_code_free_all();

    mp_hal_stdout_tx_str("MPY: soft reboot\r\n");

    // deinitialise peripherals
    machine_pwm_deinit_all();
    // TODO: machine_rmt_deinit_all();
    machine_pins_deinit();
    machine_deinit();
    #if MICROPY_PY_SOCKET_EVENTS
    socket_events_deinit();
    #endif

    mp_deinit();
    fflush(stdout);
    goto soft_reset;
}

void boardctrl_startup(void) {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        nvs_flash_erase();
        nvs_flash_init();
    }
}

void app_main(void) {
    // Hook for a board to run code at start up.
    // This defaults to initialising NVS.
    MICROPY_BOARD_STARTUP();

    // Create and transfer control to the MicroPython task.
    xTaskCreatePinnedToCore(mp_task, "mp_task", MICROPY_TASK_STACK_SIZE / sizeof(StackType_t), NULL, MP_TASK_PRIORITY, &mp_main_task_handle, MP_TASK_COREID);
    
}

// 各个模块的清理函数声明和实现
__attribute__((weak)) void camera_deinit(void) {
    // 调用ESP32摄像头清理函数
    extern esp_err_t esp_camera_deinit(void);
    esp_camera_deinit();
}

__attribute__((weak)) void screen_deinit(void) {
    // 清理屏幕相关资源
    // 这里可以添加屏幕清理逻辑，比如关闭SPI、清理缓冲区等
}

__attribute__((weak)) void sensors_deinit(void) {
    // 清理传感器相关资源
    // 这里可以添加传感器清理逻辑，比如关闭I2C、清理中断等
}

__attribute__((weak)) void rgb_deinit(void) {
    // 清理RGB LED相关资源
    // 这里可以添加RGB LED清理逻辑，比如关闭PWM、清理定时器等
}

__attribute__((weak)) void button_deinit(void) {
    // 清理按键相关资源
    // 这里可以添加按键清理逻辑，比如关闭GPIO中断等
}

__attribute__((weak)) void audio_deinit(void) {
    // 清理音频相关资源
    // 这里可以添加音频清理逻辑，比如关闭I2S、清理DMA等
}

__attribute__((weak)) void i2c_deinit(void) {
    // 清理I2C总线相关资源
    // 这里可以添加I2C清理逻辑，比如关闭I2C总线等
}

__attribute__((weak)) void spi_deinit(void) {
    // 清理SPI总线相关资源
    // 这里可以添加SPI清理逻辑，比如关闭SPI总线等
}

// 自定义资源清理函数，在Ctrl+C时调用
__attribute__((weak)) void mp_cleanup_resources_on_interrupt(void) {
    mp_hal_stdout_tx_str("\r\n[MAIN] 开始清理系统资源...\r\n");
    
    // 1. 清理所有定时器
    mp_hal_stdout_tx_str("清理定时器...\r\n");
    machine_timer_deinit_all();
    
    // 2. 清理PWM
    mp_hal_stdout_tx_str("清理PWM...\r\n");
    machine_pwm_deinit_all();
    
    // 3. 清理引脚中断
    mp_hal_stdout_tx_str("清理引脚中断...\r\n");
    machine_pins_deinit();
    
    // 4. 清理摄像头资源
    mp_hal_stdout_tx_str("清理摄像头...\r\n");
    extern void camera_deinit(void);
    camera_deinit();
    
    // 5. 清理屏幕资源
    mp_hal_stdout_tx_str("清理屏幕...\r\n");
    extern void screen_deinit(void);
    screen_deinit();
    
    // 6. 清理传感器资源
    mp_hal_stdout_tx_str("清理传感器...\r\n");
    extern void sensors_deinit(void);
    sensors_deinit();
    
    // 7. 清理RGB LED
    mp_hal_stdout_tx_str("清理RGB LED...\r\n");
    extern void rgb_deinit(void);
    rgb_deinit();
    
    // 8. 清理按键中断
    mp_hal_stdout_tx_str("清理按键中断...\r\n");
    extern void button_deinit(void);
    button_deinit();
    
    // 9. 清理音频资源
    mp_hal_stdout_tx_str("清理音频资源...\r\n");
    extern void audio_deinit(void);
    audio_deinit();
    
    // 10. 清理I2C总线
    mp_hal_stdout_tx_str("清理I2C总线...\r\n");
    extern void i2c_deinit(void);
    i2c_deinit();
    
    // 11. 清理SPI总线
    mp_hal_stdout_tx_str("清理SPI总线...\r\n");
    extern void spi_deinit(void);
    spi_deinit();
    
    // 12. 如果有自定义的AI系统，也进行清理
    #ifdef MICROPY_K10_AI_ENABLED
    mp_hal_stdout_tx_str("清理AI系统...\r\n");
    extern void mp_deinit_ai(void);
    mp_deinit_ai();
    #endif
    
    // 13. 清理其他可能的资源
    mp_hal_stdout_tx_str("执行垃圾回收...\r\n");
    gc_collect();
    
    mp_hal_stdout_tx_str("资源清理完成\r\n");
}

void nlr_jump_fail(void *val) {
    printf("NLR jump failed, val=%p\n", val);
    esp_restart();
}

static void esp_native_code_free_all(void) {
    while (native_code_head != NULL) {
        native_code_node_t *next = native_code_head->next;
        heap_caps_free(native_code_head);
        native_code_head = next;
    }
}

void *esp_native_code_commit(void *buf, size_t len, void *reloc) {
    len = (len + 3) & ~3;
    size_t len_node = sizeof(native_code_node_t) + len;
    native_code_node_t *node = heap_caps_malloc(len_node, MALLOC_CAP_EXEC);
    #if CONFIG_IDF_TARGET_ESP32S2
    // Workaround for ESP-IDF bug https://github.com/espressif/esp-idf/issues/14835
    if (node != NULL && !esp_ptr_executable(node)) {
        free(node);
        node = NULL;
    }
    #endif // CONFIG_IDF_TARGET_ESP32S2
    if (node == NULL) {
        m_malloc_fail(len_node);
    }
    node->next = native_code_head;
    native_code_head = node;
    void *p = node->data;
    if (reloc) {
        mp_native_relocate(reloc, buf, (uintptr_t)p);
    }
    memcpy(p, buf, len);
    return p;
}

