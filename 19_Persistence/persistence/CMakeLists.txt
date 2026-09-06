cmake_minimum_required(VERSION 3.13)

if(NOT DEFINED ENV{PICO_SDK_PATH} AND NOT DEFINED PICO_SDK_PATH)
    message(FATAL_ERROR "PICO_SDK_PATH is not set.")
endif()
if(NOT DEFINED ENV{PICO_EXTRAS_PATH} AND NOT DEFINED PICO_EXTRAS_PATH)
    message(FATAL_ERROR "PICO_EXTRAS_PATH is not set.")
endif()

set(PICO_BOARD pico2 CACHE STRING "Pico 2 / RP2350")

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(persistence C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

if (NOT PICO_PLATFORM STREQUAL "rp2350-arm-s")
    message(FATAL_ERROR "PERSISTENCE is RP2350 Cortex-M33 only (got ${PICO_PLATFORM}).")
endif()

file(GLOB FX_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/fx_*.c)

add_executable(persistence
    main.c
    vga.c
    audio_pwm.c
    beam.c
    timeline.c
    tables.c
    synth.c
    song.c
    s3d.c
    mesh.c
    affine.c
    text.c
    ${FX_SOURCES}
)

target_include_directories(persistence PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
# Development switches, never set in a release build:
#   -DPV_SOLO=n   run timeline cue n alone from frame 0
#   -DPV_PROF=n   remove work from a kernel to find where its cycles go
set(PV_SOLO "" CACHE STRING "run one cue alone")
set(PV_PROF "0" CACHE STRING "profiling stub level")
target_compile_definitions(persistence PRIVATE PICO_BUILD=1 PV_PROF=${PV_PROF})
if(NOT PV_SOLO STREQUAL "")
    target_compile_definitions(persistence PRIVATE PV_SOLO=${PV_SOLO})
endif()

# -fno-math-errno lets the compiler use the FPU's own square root instead of a
# libm call that also has to set errno. It does NOT license reassociation the
# way -ffast-math would, which matters because the host and the device are
# diffed against each other.
target_compile_options(persistence PRIVATE -fno-math-errno)
target_compile_options(persistence PRIVATE -O2)

target_link_libraries(persistence
    pico_stdlib
    pico_scanvideo_dpi
    pico_multicore
    hardware_pwm
    hardware_dma
    hardware_interp
)

target_compile_definitions(persistence PRIVATE
    PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=324
    PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=16
    PICO_SCANVIDEO_MISSING_SCANLINE_COLOR=0
    PICO_STACK_SIZE=0x1000
    PICO_CORE1_STACK_SIZE=0x800
)

pico_enable_stdio_usb(persistence 1)
pico_enable_stdio_uart(persistence 0)

pico_add_extra_outputs(persistence)
