# ESP32-S3专用用户模块配置
# 只包含K10项目需要的模块

message("ESP32-S3专用用户模块配置")

# K10摄像头模块
include(${CMAKE_CURRENT_LIST_DIR}/k10_cam/micropython.cmake)

# K10 LVGL绑定
include(${CMAKE_CURRENT_LIST_DIR}/lv_binding_micropython/micropython.cmake)

# K10人脸识别模块
include(${CMAKE_CURRENT_LIST_DIR}/k10_face/micropython.cmake)

# K10语音识别模块
include(${CMAKE_CURRENT_LIST_DIR}/k10_asr/micropython.cmake)
