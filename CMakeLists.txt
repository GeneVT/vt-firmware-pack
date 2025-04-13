cmake_minimum_required(VERSION 3.5)
##project(vt-fitness VERSION 2.0.0 LANGUAGES CXX)
project(vt-firmware-pack VERSION 2.0.0 LANGUAGES CXX)

set(APP_NAME vt-shadow-publish)
set(APP_LIB_NAME firmware_update_lib)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_FLAGS "-Wall -Werror -Wextra -O2 -s")

set (CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/out)

include(CTest)

add_subdirectory(src)

add_subdirectory(tests)

