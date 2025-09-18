# This top-level micropython.cmake is responsible for listing
# the individual modules we want to include.
# Paths are absolute, and ${CMAKE_CURRENT_LIST_DIR} can be
# used to prefix subdirectories.

# Add the C example.
#include(${CMAKE_CURRENT_LIST_DIR}/cexample/micropython.cmake)

# Add the CPP example.
#include(${CMAKE_CURRENT_LIST_DIR}/cppexample/micropython.cmake)
message("userCmodule from CMake!")

# Add the K10 CAM.
include(${CMAKE_CURRENT_LIST_DIR}/k10_cam/micropython.cmake)

#Add the K10 AI.
#include(${CMAKE_CURRENT_LIST_DIR}/k10_ai/micropython.cmake)

#Add the K10 lvgl
include(${CMAKE_CURRENT_LIST_DIR}/lv_binding_micropython/micropython.cmake)

#Add the K10 Face.
include(${CMAKE_CURRENT_LIST_DIR}/k10_face/micropython.cmake)

#Add the K10 ASR.
include(${CMAKE_CURRENT_LIST_DIR}/k10_asr/micropython.cmake)




