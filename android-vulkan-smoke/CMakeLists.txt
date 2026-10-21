cmake_minimum_required(VERSION 3.22)
project(androidwine_vk_probe C)

if(NOT CMAKE_ANDROID_NDK)
    message(FATAL_ERROR "Build this project with the Android NDK CMake toolchain")
endif()

set(NATIVE_APP_GLUE "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue/android_native_app_glue.c")
if(NOT EXISTS "${NATIVE_APP_GLUE}")
    message(FATAL_ERROR "Android native_app_glue.c not found: ${NATIVE_APP_GLUE}")
endif()

add_library(androidwine_vk_probe SHARED
    src/main/cpp/androidwine_vk_probe.c
    "${NATIVE_APP_GLUE}"
)
target_include_directories(androidwine_vk_probe PRIVATE
    "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue"
)
target_compile_features(androidwine_vk_probe PRIVATE c_std_11)
target_compile_options(androidwine_vk_probe PRIVATE -Wall -Wextra -Werror=return-type)

find_library(ANDROID_LIB android REQUIRED)
find_library(LOG_LIB log REQUIRED)
find_library(VULKAN_LIB vulkan REQUIRED)
find_library(M_LIB m REQUIRED)
target_link_libraries(androidwine_vk_probe PRIVATE
    "${ANDROID_LIB}"
    "${LOG_LIB}"
    "${VULKAN_LIB}"
    "${M_LIB}"
)
