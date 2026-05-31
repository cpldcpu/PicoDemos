cmake_minimum_required(VERSION 3.13)

# PICO_SDK_PATH and PICO_EXTRAS_PATH must be set in the environment (or
# passed via -D). See IMPLEMENTATION.md for setup.
if(NOT DEFINED ENV{PICO_SDK_PATH} AND NOT DEFINED PICO_SDK_PATH)
    message(FATAL_ERROR "PICO_SDK_PATH is not set. Point it at your pico-sdk checkout.")
endif()
if(NOT DEFINED ENV{PICO_EXTRAS_PATH} AND NOT DEFINED PICO_EXTRAS_PATH)
    message(FATAL_ERROR "PICO_EXTRAS_PATH is not set. Point it at your pico-extras checkout.")
endif()

# RP2350-only — we lean on the M33 FPU for the polygon transform/shade math.
set(PICO_BOARD pico2 CACHE STRING "Pico 2 / RP2350")

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(origami C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

if (NOT PICO_PLATFORM STREQUAL "rp2350-arm-s" AND NOT PICO_PLATFORM STREQUAL "rp2350-riscv")
    message(FATAL_ERROR "14_Origami is RP2350-only. Got PICO_PLATFORM=${PICO_PLATFORM}.")
endif()

add_executable(origami
    main.c
    vga.c
    scene.c
    scene_scratch.c
    timeline.c
    audio_qoa.c
    music_qoa.S

    effects/poly3d.c
    effects/title.c
    effects/plane.c
    effects/crane.c
    effects/city.c
    effects/miura.c
    effects/credits.c
)

target_include_directories(origami PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

# music_qoa.S incbins music.qoa (assets/Marimba Seedbox.mp3 -> mono 22050 Hz
# s16 -> qoaconv_s16 -> music.qoa; see IMPLEMENTATION.md for the pipeline).
set_source_files_properties(music_qoa.S PROPERTIES
    COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}"
    OBJECT_DEPENDS  "${CMAKE_CURRENT_SOURCE_DIR}/music.qoa"
)

target_compile_definitions(origami PRIVATE PICO_BUILD=1)

target_link_libraries(origami
    pico_stdlib
    pico_scanvideo_dpi
    pico_multicore
    hardware_pwm
    hardware_dma
)

target_compile_definitions(origami PRIVATE
    PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=400
    PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=16
)

pico_enable_stdio_usb(origami 1)
pico_enable_stdio_uart(origami 0)

pico_add_extra_outputs(origami)
