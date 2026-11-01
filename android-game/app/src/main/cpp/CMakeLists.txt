cmake_minimum_required(VERSION 3.22.1)
project(imisubebe LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(ANDROID)
    set(NATIVE_APP_GLUE_DIR "${CMAKE_ANDROID_NDK}/sources/android/native_app_glue")
    add_library(native_app_glue STATIC "${NATIVE_APP_GLUE_DIR}/android_native_app_glue.c")
    target_include_directories(native_app_glue PUBLIC "${NATIVE_APP_GLUE_DIR}")

    add_library(imisubebe SHARED
        audio.cpp
        game.cpp
        main.cpp
        map_data.cpp
        renderer.cpp
    )
    target_include_directories(imisubebe PRIVATE "${NATIVE_APP_GLUE_DIR}")
    target_compile_options(imisubebe PRIVATE
        -Wall -Wextra -Wpedantic
        -Wno-unused-parameter
        -ffast-math
    )
    target_link_libraries(imisubebe PRIVATE
        native_app_glue
        android
        log
        EGL
        GLESv3
        aaudio
        m
        z
    )
else()
    enable_testing()
    add_executable(core-tests
        game.cpp
        map_data.cpp
        tests/core_tests.cpp
    )
    target_compile_options(core-tests PRIVATE -Wall -Wextra -Wpedantic)
    add_test(
        NAME native-game-core
        COMMAND core-tests "${CMAKE_CURRENT_SOURCE_DIR}/../assets/osm_cebu.osm"
    )
endif()
