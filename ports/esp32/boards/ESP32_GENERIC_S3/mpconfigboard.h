#ifndef MICROPY_HW_BOARD_NAME
// Can be set by mpconfigboard.cmake.
#define MICROPY_HW_BOARD_NAME               "Generic ESP32S3 module"
#endif
#define MICROPY_HW_MCU_NAME                 "ESP32S3"

// Enable UART REPL for modules that have an external USB-UART and don't use native USB.
#define MICROPY_HW_ENABLE_UART_REPL         (0)

// Use USB Serial/JTAG as the MicroPython REPL channel.
// This keeps flashing/debug and user REPL on the same USB path.
#define MICROPY_HW_ENABLE_USBDEV            (0)
#define MICROPY_HW_ENABLE_USB_RUNTIME_DEVICE (0)
#define MICROPY_HW_USB_CDC                  (0)
#define MICROPY_HW_ESP_USB_SERIAL_JTAG      (1)

#define MICROPY_HW_I2C0_SCL                 (48)
#define MICROPY_HW_I2C0_SDA                 (47)

#define MODULE_CAMERA_ENABLED               (1)// 开启camera
