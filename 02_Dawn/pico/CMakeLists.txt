cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED ENV{PICO_SDK_PATH})
    set(ENV{PICO_SDK_PATH} "D:/Pico/pico-sdk")
endif()
if(NOT DEFINED ENV{PICO_EXTRAS_PATH})
    set(ENV{PICO_EXTRAS_PATH} "D:/Pico/pico-extras")
endif()

# Default board = RP2040 Pico. To build for RP2350 use a fresh build dir
# and configure with -DPICO_BOARD=pico2 (or waveshare_rp2350_plus_4mb etc).
set(PICO_BOARD pico CACHE STRING "Board (pico = RP2040, pico2 = RP2350)")

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(dawn_pico C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

add_executable(dawn_pico
    main.c
    vga.c
    audio.c
    chunky.c
    palette.c
    mathtab.c
    vector3d.c
    torus.c
    texmap.c
    voxel.c
    effects.c
    button.c
    sequencer.c
)

target_include_directories(dawn_pico PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_compile_definitions(dawn_pico PRIVATE
    PICO_BUILD=1
)

target_compile_options(dawn_pico PRIVATE
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-maybe-uninitialized
)

target_link_libraries(dawn_pico
    pico_stdlib
    pico_scanvideo_dpi
    pico_multicore
    hardware_pwm
    hardware_dma
)

# Scanvideo tuning: 200-word-max scanline buffers (Pimoroni VGA layout),
# 16 buffers — same defaults the SDK example ships with. 320x240 at 60 Hz
# fits comfortably.
target_compile_definitions(dawn_pico PRIVATE
    PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=200
    PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=16
)

pico_add_extra_outputs(dawn_pico)

# USB-CDC stdio for printf. UART disabled — GP0/GP1 are red-bit-0/1 on
# the VGA base.
pico_enable_stdio_usb(dawn_pico 1)
pico_enable_stdio_uart(dawn_pico 0)
