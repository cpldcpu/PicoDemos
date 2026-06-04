cmake_minimum_required(VERSION 3.13)

# PICO_SDK_PATH and PICO_EXTRAS_PATH must be set in the environment (or -D).
if(NOT DEFINED ENV{PICO_SDK_PATH} AND NOT DEFINED PICO_SDK_PATH)
    message(FATAL_ERROR "PICO_SDK_PATH is not set. Point it at your pico-sdk checkout.")
endif()
if(NOT DEFINED ENV{PICO_EXTRAS_PATH} AND NOT DEFINED PICO_EXTRAS_PATH)
    message(FATAL_ERROR "PICO_EXTRAS_PATH is not set. Point it at your pico-extras checkout.")
endif()

# RP2350-only — the demo's hero is the RP2350 SIO interpolator, and we lean on
# the M33 FPU for the per-frame transform/affine setup.
set(PICO_BOARD pico2 CACHE STRING "Pico 2 / RP2350")

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(quicksilver C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

if (NOT PICO_PLATFORM STREQUAL "rp2350-arm-s" AND NOT PICO_PLATFORM STREQUAL "rp2350-riscv")
    message(FATAL_ERROR "15_Quicksilver is RP2350-only. Got PICO_PLATFORM=${PICO_PLATFORM}.")
endif()

add_executable(quicksilver
    main.c
    vga.c
    scene.c
    scene_scratch.c
    timeline.c
    audio_qoa.c
    music_qoa.S
    assets/_packed/assets.S

    effects/rotozoom.c
    effects/mode7.c
    effects/envmap3d.c
    effects/chrome.c
    effects/liquid.c
    effects/tunnel.c
    effects/title.c
    effects/credits.c
    effects/transition.c
)
# NOTE: interp_emu.c is the HOST-only software emulator and is deliberately NOT
# listed here — the firmware uses the real SIO interpolator (hardware/interp.h).

target_include_directories(quicksilver PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed
)

# assets.S incbins each <name>.bin / <name>.pal from assets/_packed/.
set_source_files_properties(assets/_packed/assets.S PROPERTIES
    COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed"
    OBJECT_DEPENDS  "${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/roto.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/ground.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/envmap.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/sky.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/title_bg.bin"
)

# music_qoa.S incbins music.qoa.
set_source_files_properties(music_qoa.S PROPERTIES
    COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}"
    OBJECT_DEPENDS  "${CMAKE_CURRENT_SOURCE_DIR}/music.qoa"
)

target_compile_definitions(quicksilver PRIVATE PICO_BUILD=1)

target_link_libraries(quicksilver
    pico_stdlib
    pico_scanvideo_dpi
    pico_multicore
    hardware_pwm
    hardware_dma
    hardware_interp      # the SIO interpolators — QUICKSILVER's hardware hero
)

target_compile_definitions(quicksilver PRIVATE
    PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=400
    PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=16
)

pico_enable_stdio_usb(quicksilver 1)
pico_enable_stdio_uart(quicksilver 0)

pico_add_extra_outputs(quicksilver)

# (The standalone full-VGA 640x480 beam-raced rotozoom lived here as a separate
# quicksilver_vga640 .uf2; it was a hardware test bench and is no longer built.
# The technique now ships inside the demo's rotozoomer scene. The vga640/
# sources remain in the tree for reference.)
