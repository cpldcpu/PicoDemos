cmake_minimum_required(VERSION 3.13)

# PICO_SDK_PATH and PICO_EXTRAS_PATH must be set in the environment (or -D).
if(NOT DEFINED ENV{PICO_SDK_PATH} AND NOT DEFINED PICO_SDK_PATH)
    message(FATAL_ERROR "PICO_SDK_PATH is not set. Point it at your pico-sdk checkout.")
endif()
if(NOT DEFINED ENV{PICO_EXTRAS_PATH} AND NOT DEFINED PICO_EXTRAS_PATH)
    message(FATAL_ERROR "PICO_EXTRAS_PATH is not set. Point it at your pico-extras checkout.")
endif()

set(PICO_BOARD pico2 CACHE STRING "Pico 2 / RP2350")

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(sustain C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

if (NOT PICO_PLATFORM STREQUAL "rp2350-arm-s" AND NOT PICO_PLATFORM STREQUAL "rp2350-riscv")
    message(FATAL_ERROR "16_Sustain is RP2350-only. Got PICO_PLATFORM=${PICO_PLATFORM}.")
endif()

add_executable(sustain
    main.c
    vga.c
    world.c
    arc.c
    render.c
    field_scratch.c
    motes.c
    overlay.c
    mip.c
    audio_qoa.c
    music_qoa.S
    assets/_packed/assets.S

    fields/f_terrain.c
    fields/f_cave.c
    fields/f_monolith.c
    fields/sky_common.c
)
# NOTE: interp_emu.c is the HOST-only software emulator and is deliberately NOT
# listed here — the firmware uses the real SIO interpolator (hardware/interp.h).

target_include_directories(sustain PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed
)

set_source_files_properties(assets/_packed/assets.S PROPERTIES
    COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed"
    OBJECT_DEPENDS  "${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/surface_cold.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/surface_hot.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/wall_cold.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/wall_hot.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/relief_soft.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/relief_hard.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/sky_pano.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/matcap_cold.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/matcap_warm.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/sustain_logo.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/latent_logo.bin"
)

set_source_files_properties(music_qoa.S PROPERTIES
    COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}"
    OBJECT_DEPENDS  "${CMAKE_CURRENT_SOURCE_DIR}/music.qoa"
)

target_compile_definitions(sustain PRIVATE PICO_BUILD=1)

# The ray walk is the whole demo, so it gets the aggressive flags. -ffast-math
# is safe here: nothing in the renderer depends on NaN/Inf semantics or on
# strict IEEE association, and reciprocal/rsqrt substitution is worth real
# frames in a loop that divides by view distance at every step.
target_compile_options(sustain PRIVATE
    -O3 -ffast-math -fno-math-errno -funroll-loops
)

target_link_libraries(sustain
    pico_stdlib
    pico_scanvideo_dpi
    pico_multicore
    hardware_pwm
    hardware_dma
    hardware_interp
)

target_compile_definitions(sustain PRIVATE
    PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=400
    PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=16
)

pico_enable_stdio_usb(sustain 1)

# UART stdio as well, as a fallback when USB CDC does not enumerate.
# NOT on the default GP0/GP1: the Pimoroni VGA base drives GP0..GP15 as the
# RGB DAC, so the stock UART pins would corrupt the picture. GP16/GP17 are
# clear of both the video pins and the audio pins (GP26..GP28).
pico_enable_stdio_uart(sustain 1)
target_compile_definitions(sustain PRIVATE
    PICO_DEFAULT_UART_TX_PIN=16
    PICO_DEFAULT_UART_RX_PIN=17
)

pico_add_extra_outputs(sustain)
