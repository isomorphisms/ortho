cmake_minimum_required(VERSION 3.22.1)
project(ortho C)

set(NATIVE_GLUE_DIR "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")

add_library(native_app_glue STATIC
    "${NATIVE_GLUE_DIR}/android_native_app_glue.c"
)
target_include_directories(native_app_glue PUBLIC "${NATIVE_GLUE_DIR}")

add_library(ortho SHARED ortho.c)
target_compile_features(ortho PRIVATE c_std_11)
target_compile_options(ortho PRIVATE -Wall -Wextra -Wpedantic)
target_include_directories(ortho PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_options(ortho PRIVATE -u ANativeActivity_onCreate)
target_link_libraries(ortho
    native_app_glue
    android
    log
    EGL
    GLESv3
    m
)
