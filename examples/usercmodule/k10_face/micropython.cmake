# 创建用户模块库
add_library(usermod_K10_face INTERFACE)

# 添加源文件
target_sources(usermod_K10_face INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}/k10_face.cpp
    ${CMAKE_CURRENT_LIST_DIR}/k10_face.c
)

# 设置包含目录
target_include_directories(usermod_K10_face INTERFACE
    ${CMAKE_CURRENT_LIST_DIR}
    ${IDF_PATH}/components/esp-dl/include
    ${IDF_PATH}/components/esp-dl/include/image
    ${IDF_PATH}/components/esp-dl/include/tool
    ${IDF_PATH}/components/esp-dl/include/typedef
    ${IDF_PATH}/components/esp-dl/include/math
    ${IDF_PATH}/components/esp-dl/include/nn
    ${IDF_PATH}/components/esp-dl/include/tvm
    ${IDF_PATH}/components/esp-dl/include/layer
    ${IDF_PATH}/components/esp-dl/include/detect
    ${IDF_PATH}/components/esp-dl/include/model_zoo
    ${IDF_PATH}/components/esp-code-scanner/include
    ${IDF_PATH}/components/esp32-camera/driver/include
    ${IDF_PATH}/components/esp32-camera/driver/private_include
)

# 启用C++异常和RTTI - 只对C++文件
target_compile_options(usermod_K10_face INTERFACE
    $<$<COMPILE_LANGUAGE:CXX>:-fexceptions>
    $<$<COMPILE_LANGUAGE:CXX>:-frtti>
    $<$<COMPILE_LANGUAGE:CXX>:-std=gnu++17>
    $<$<COMPILE_LANGUAGE:CXX>:-D_GNU_SOURCE>
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-gnu-zero-variadic-macro-arguments>
)

target_compile_definitions(usermod_K10_face INTERFACE
    CONFIG_IDF_TARGET_ESP32S3
    ESP_PLATFORM
    ESP32
    ESP32S3
    CONFIG_ESP32S3_DEFAULT_CPU_FREQ_240
)

# 链接到主用户模块
target_link_libraries(usermod INTERFACE usermod_K10_face)




