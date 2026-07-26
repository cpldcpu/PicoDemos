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
    score.c
    palette.c
    # rd.c is NOT linked: reaction-diffusion is compiled out (HYST_RD_ENABLE in
    # sim.c) and its state is 153 KB of bss, which took the device from 318 KB
    # to 491 KB of 520 and left only ~41 KB for stacks. Dead weight eating the
    # margin. Re-add this line when the define goes back to 1.
    audio_stub.c
)

target_include_directories(hysteresis PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# HYST_PROF locates the frame cost by removing work (see field.c). Pass
# -DHYST_PROF=1 or 2 at configure time; never overriding CMAKE_C_FLAGS, which
# would discard the SDK's own required flags.
set(HYST_PROF "0" CACHE STRING "profiling stub level")
target_compile_definitions(hysteresis PRIVATE PICO_BUILD=1 HYST_PROF=${HYST_PROF})

# -O3 and unrolling, but NOT -ffast-math, which SUSTAIN used.
#
# Nothing here is floating point: the simulation is integer by design
# (field.h), precisely so that a host render and a device render can be diffed
# byte for byte. -ffast-math would be inert at best, and licensing the compiler
# to reassociate arithmetic is the wrong signal to send in a file whose whole
# contract is bit-exact reproducibility.
target_compile_options(hysteresis PRIVATE
    # NO -funroll-loops. It doubles register pressure in the convolve loop,
    # which the disassembly showed was already spilling five-plus times per
    # cell -- unrolling made the real bottleneck worse while looking like an
    # optimisation.
    -O3
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
