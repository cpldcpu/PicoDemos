cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED ENV{PICO_SDK_PATH})
    set(ENV{PICO_SDK_PATH} "D:/pico/pico-sdk")
endif()
if(NOT DEFINED ENV{PICO_EXTRAS_PATH})
    set(ENV{PICO_EXTRAS_PATH} "D:/pico/pico-extras")
endif()

# Default to RP2040 (`pico`). To build for RP2350 (Pico 2), configure
# with `cmake -DPICO_BOARD=pico2 ..` in a fresh build directory. The
# same C sources work on both platforms; the SDK abstracts the chip
# differences and main.c / sound_qoa.c read the live sys_clk so the
# 250 MHz overclock and 11025 Hz audio divisor are correct on either.
set(PICO_BOARD pico CACHE STRING "Board")

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(sota_pico C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

# RP2350 has 520 KB SRAM vs RP2040's 264 KB — plenty of headroom for a
# bigger engine heap on the VGA build, which is otherwise the most RAM-
# constrained target (scanvideo buffers + 8 KB audio ring + 4 font
# bitplanes). 64 KB matches the ST7789 build and removes the malloc-once
# font dance the RP2040 had to do.
if (PICO_PLATFORM STREQUAL "rp2350-arm-s" OR PICO_PLATFORM STREQUAL "rp2350-riscv")
    set(SOTA_VGA_HEAP_KB 64)
    message(STATUS "SOTA: RP2350 target, VGA heap = ${SOTA_VGA_HEAP_KB} KB")
else()
    set(SOTA_VGA_HEAP_KB 16)
    message(STATUS "SOTA: RP2040 target, VGA heap = ${SOTA_VGA_HEAP_KB} KB")
endif()

set(SOTA_NATIVE ${CMAKE_CURRENT_SOURCE_DIR}/../native)

add_executable(sota_pico
    main.c
    pico_st7789_backend.c
    sound_stub.c
    bitplane_compose.c
    st7789.c
    sota_wad.S

    # SOTA engine — same .c files the desktop build uses.
    ${SOTA_NATIVE}/anim.c
    ${SOTA_NATIVE}/scene.c
    ${SOTA_NATIVE}/choreography.c
    ${SOTA_NATIVE}/graphics.c
    ${SOTA_NATIVE}/iff.c
    ${SOTA_NATIVE}/iff-font.c
    ${SOTA_NATIVE}/wad.c
    ${SOTA_NATIVE}/heap.c
    ${SOTA_NATIVE}/mbit.c
    ${SOTA_NATIVE}/pcgrandom.c

    ${SOTA_NATIVE}/tinf/src/adler32.c
    ${SOTA_NATIVE}/tinf/src/crc32.c
    ${SOTA_NATIVE}/tinf/src/tinflate.c
    ${SOTA_NATIVE}/tinf/src/tinfzlib.c
)

target_include_directories(sota_pico PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}     # pico_endian.h, st7789.h, bitplane_compose.h
    ${SOTA_NATIVE}                  # backend.h, sound.h, anim.h, ...
    ${SOTA_NATIVE}/tinf/src
)

# Tell the assembler where to find sota.wad (used by sota_wad.S's .incbin).
set_source_files_properties(sota_wad.S PROPERTIES
    COMPILE_OPTIONS "-I${SOTA_NATIVE}"
)

target_compile_definitions(sota_pico PRIVATE
    HEAP_SIZE_KB=64
    BACKEND_SUPPORTS_SOUND=1
    BACKEND_SUPPORTS_ILBM=1
    PICO_BUILD=1
)

# Engine source pulls -Werror habits from desktop; on the Pico we're
# fighting newer gcc warnings we don't care about.
target_compile_options(sota_pico PRIVATE
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-maybe-uninitialized
)

target_link_libraries(sota_pico
    pico_stdlib
    hardware_spi
    hardware_dma
)

pico_add_extra_outputs(sota_pico)

pico_enable_stdio_usb(sota_pico 0)
pico_enable_stdio_uart(sota_pico 1)


# ----------------------------------------------------------------------
# Stand-alone composite/S-Video Y test pattern. 1-bit B/W vertical bars.
# No engine, no wad, no display driver — just PIO + DMA streaming a
# pre-baked frame buffer to GP14 (video, 1k) and GP15 (sync, 430R).
# ----------------------------------------------------------------------
add_executable(sota_composite_test
    composite_test.c
)

pico_generate_pio_header(sota_composite_test
    ${CMAKE_CURRENT_SOURCE_DIR}/composite.pio
)

target_link_libraries(sota_composite_test
    pico_stdlib
    hardware_pio
    hardware_dma
    hardware_clocks
)

pico_add_extra_outputs(sota_composite_test)
pico_enable_stdio_usb(sota_composite_test 0)
pico_enable_stdio_uart(sota_composite_test 1)


# ----------------------------------------------------------------------
# Engine-integrated composite output: 320x256@50 Hz PAL, 1-bit luma with
# 4x4 Bayer ordered dithering for fake greyscale. Same wad and engine
# sources as sota_pico, just a different backend.h implementation.
# ----------------------------------------------------------------------
add_executable(sota_composite
    main.c
    pico_composite_backend.c
    sound_stub.c
    sota_wad.S

    ${SOTA_NATIVE}/anim.c
    ${SOTA_NATIVE}/scene.c
    ${SOTA_NATIVE}/choreography.c
    ${SOTA_NATIVE}/graphics.c
    ${SOTA_NATIVE}/iff.c
    ${SOTA_NATIVE}/iff-font.c
    ${SOTA_NATIVE}/wad.c
    ${SOTA_NATIVE}/heap.c
    ${SOTA_NATIVE}/mbit.c
    ${SOTA_NATIVE}/pcgrandom.c

    ${SOTA_NATIVE}/tinf/src/adler32.c
    ${SOTA_NATIVE}/tinf/src/crc32.c
    ${SOTA_NATIVE}/tinf/src/tinflate.c
    ${SOTA_NATIVE}/tinf/src/tinfzlib.c
)

pico_generate_pio_header(sota_composite
    ${CMAKE_CURRENT_SOURCE_DIR}/composite.pio
)

target_include_directories(sota_composite PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SOTA_NATIVE}
    ${SOTA_NATIVE}/tinf/src
)

set_source_files_properties(sota_wad.S PROPERTIES
    COMPILE_OPTIONS "-I${SOTA_NATIVE}"
)

target_compile_definitions(sota_composite PRIVATE
    HEAP_SIZE_KB=20
    BACKEND_SUPPORTS_SOUND=1
    BACKEND_SUPPORTS_ILBM=1
    PICO_BUILD=1
)

target_compile_options(sota_composite PRIVATE
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-maybe-uninitialized
)

target_link_libraries(sota_composite
    pico_stdlib
    hardware_pio
    hardware_dma
    hardware_clocks
)

pico_add_extra_outputs(sota_composite)
pico_enable_stdio_usb(sota_composite 0)
pico_enable_stdio_uart(sota_composite 1)


# ----------------------------------------------------------------------
# VGA output via Pimoroni Pico VGA Demo Base. Uses pico_scanvideo from
# pico-extras with its default RPi-Foundation pinout (R=GP0-4, G=GP6-10,
# B=GP11-15, HSYNC=GP16, VSYNC=GP17). Engine renders 6 bitplanes -> 32K
# colors per pixel directly into scanvideo buffers.
# ----------------------------------------------------------------------
add_executable(sota_vga
    main.c
    pico_vga_backend.c
    sound_qoa.c
    sota_wad_qoa.S

    ${SOTA_NATIVE}/anim.c
    ${SOTA_NATIVE}/scene.c
    ${SOTA_NATIVE}/choreography.c
    ${SOTA_NATIVE}/graphics.c
    ${SOTA_NATIVE}/iff.c
    ${SOTA_NATIVE}/iff-font.c
    ${SOTA_NATIVE}/wad.c
    ${SOTA_NATIVE}/heap.c
    ${SOTA_NATIVE}/mbit.c
    ${SOTA_NATIVE}/pcgrandom.c

    ${SOTA_NATIVE}/tinf/src/adler32.c
    ${SOTA_NATIVE}/tinf/src/crc32.c
    ${SOTA_NATIVE}/tinf/src/tinflate.c
    ${SOTA_NATIVE}/tinf/src/tinfzlib.c
)

target_include_directories(sota_vga PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${SOTA_NATIVE}
    ${SOTA_NATIVE}/tinf/src
)

# Tell the assembler where to find sota_pico.wad (used by sota_wad_qoa.S).
set_source_files_properties(sota_wad_qoa.S PROPERTIES
    COMPILE_OPTIONS "-I${SOTA_NATIVE}"
)

target_compile_definitions(sota_vga PRIVATE
    HEAP_SIZE_KB=${SOTA_VGA_HEAP_KB}
    BACKEND_SUPPORTS_SOUND=1
    BACKEND_SUPPORTS_ILBM=1
    PICO_BUILD=1
    SOTA_VGA_BUILD=1
    PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=200
    PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=32
)

target_compile_options(sota_vga PRIVATE
    -Wno-unused-function
    -Wno-unused-variable
    -Wno-unused-but-set-variable
    -Wno-maybe-uninitialized
)

target_link_libraries(sota_vga
    pico_stdlib
    pico_scanvideo_dpi
    pico_multicore
    hardware_pwm
    hardware_dma
)

pico_add_extra_outputs(sota_vga)
# UART disabled (GP0/GP1 are red bits 0/1). USB CDC stdio is fine — it
# uses the dedicated USB pins, not any VGA GPIO. Used for printf-based
# debugging of the long pre-render delay.
pico_enable_stdio_usb(sota_vga 1)
pico_enable_stdio_uart(sota_vga 0)


# ----------------------------------------------------------------------
# Standalone VGA bring-up test: no engine, no wad, just pico_scanvideo
# painting a known color-bar pattern. Used to isolate scanvideo/sync
# issues from any engine-integration bug.
# ----------------------------------------------------------------------
add_executable(sota_vga_test
    vga_test.c
)

target_compile_definitions(sota_vga_test PRIVATE
    PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=200
    PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=32
)

target_link_libraries(sota_vga_test
    pico_stdlib
    pico_scanvideo_dpi
    pico_multicore
)

pico_add_extra_outputs(sota_vga_test)
# UART disabled (GP0/GP1 are red bits 0/1). USB CDC enabled for serial
# debug — verifies the overclock + USB CDC path in isolation.
pico_enable_stdio_usb(sota_vga_test 1)
pico_enable_stdio_uart(sota_vga_test 0)
