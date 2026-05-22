cmake_minimum_required(VERSION 3.13)

# PICO_SDK_PATH and PICO_EXTRAS_PATH must be set in the environment (or
# passed via -D on the cmake command line). See README.md for setup.
if(NOT DEFINED ENV{PICO_SDK_PATH} AND NOT DEFINED PICO_SDK_PATH)
    message(FATAL_ERROR "PICO_SDK_PATH is not set. Point it at your pico-sdk checkout.")
endif()
if(NOT DEFINED ENV{PICO_EXTRAS_PATH} AND NOT DEFINED PICO_EXTRAS_PATH)
    message(FATAL_ERROR "PICO_EXTRAS_PATH is not set. Point it at your pico-extras checkout.")
endif()

# RP2350-only project. We depend on the M33 FPU for the SDF/raytracer scenes.
set(PICO_BOARD pico2 CACHE STRING "Pico 2 / RP2350")

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(thedemo C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

if (NOT PICO_PLATFORM STREQUAL "rp2350-arm-s" AND NOT PICO_PLATFORM STREQUAL "rp2350-riscv")
    message(FATAL_ERROR "VOLTAGE is RP2350-only. Got PICO_PLATFORM=${PICO_PLATFORM}.")
endif()

add_executable(thedemo
    main.c
    vga.c
    scene.c
    scene_scratch.c
    timeline.c
    audio_qoa.c
    music_qoa.S
    assets/_packed/assets.S

    effects/cpc_boot.c
    effects/plasma_chip.c
    effects/text_matrix.c
    effects/dirty_logo.c
    effects/reaction_mind.c
    effects/fractal_zoom.c
    effects/greetings.c
    effects/outro.c
)

target_include_directories(thedemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed
)

# assets.S incbins each <name>.bin / <name>.pal from assets/_packed/.
set_source_files_properties(assets/_packed/assets.S PROPERTIES
    COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed"
)

# music_qoa.S incbins music.qoa from this directory.
set_source_files_properties(music_qoa.S PROPERTIES
    COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}"
    OBJECT_DEPENDS  "${CMAKE_CURRENT_SOURCE_DIR}/music.qoa"
)

target_include_directories(thedemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(thedemo PRIVATE
    PICO_BUILD=1
)

target_link_libraries(thedemo
    pico_stdlib
    pico_scanvideo_dpi
    pico_multicore
    hardware_pwm
    hardware_dma
)

# scanvideo buffer pool.
target_compile_definitions(thedemo PRIVATE
    PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=400
    PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=16
)

# USB CDC stdio for printf. UART disabled.
pico_enable_stdio_usb(thedemo 1)
pico_enable_stdio_uart(thedemo 0)

pico_add_extra_outputs(thedemo)
