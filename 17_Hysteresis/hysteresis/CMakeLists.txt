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

project(hysteresis C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

if (NOT PICO_PLATFORM STREQUAL "rp2350-arm-s" AND NOT PICO_PLATFORM STREQUAL "rp2350-riscv")
    message(FATAL_ERROR "17_Hysteresis is RP2350-only. Got PICO_PLATFORM=${PICO_PLATFORM}.")
endif()

add_executable(hysteresis
    main.c
    vga.c
    sim.c
    field.c
    palette.c
    audio_stub.c
)

target_include_directories(hysteresis PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(hysteresis PRIVATE PICO_BUILD=1)

# -O3 and unrolling, but NOT -ffast-math, which SUSTAIN used.
#
# Nothing here is floating point: the simulation is integer by design
# (field.h), precisely so that a host render and a device render can be diffed
# byte for byte. -ffast-math would be inert at best, and licensing the compiler
# to reassociate arithmetic is the wrong signal to send in a file whose whole
# contract is bit-exact reproducibility.
target_compile_options(hysteresis PRIVATE
    -O3 -funroll-loops
)

target_link_libraries(hysteresis
    pico_stdlib
    pico_scanvideo_dpi
    pico_multicore
    hardware_pwm
    hardware_dma
)

target_compile_definitions(hysteresis PRIVATE
    PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=400
    PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=16
)

pico_enable_stdio_usb(hysteresis 1)

# UART stdio as a fallback when USB CDC does not enumerate. NOT on GP0/GP1:
# the Pimoroni VGA base drives GP0..GP15 as the RGB DAC, so the stock UART
# pins would corrupt the picture. GP16/GP17 are clear of the video pins and of
# the audio pins (GP26..GP28).
pico_enable_stdio_uart(hysteresis 1)
target_compile_definitions(hysteresis PRIVATE
    PICO_DEFAULT_UART_TX_PIN=16
    PICO_DEFAULT_UART_RX_PIN=17
)

pico_add_extra_outputs(hysteresis)
