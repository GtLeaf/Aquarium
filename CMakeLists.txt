cmake_minimum_required(VERSION 3.22)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_C_STANDARD 11)

# Set the target chip
set(IDF_TARGET esp32s3)
set(EXTRA_COMPONENT_DIRS
    ${CMAKE_SOURCE_DIR}/components
    ${CMAKE_SOURCE_DIR}/main
)

# Include ESP-IDF project CMake
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

project(esp32s3_touch_amoled_18)
