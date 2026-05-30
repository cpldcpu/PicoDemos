cmake_minimum_required(VERSION 3.13)

# PICO_SDK_PATH and PICO_EXTRAS_PATH must be set in the environment (or
# passed via -D). See IMPLEMENTATION.md for setup.
if(NOT DEFINED ENV{PICO_SDK_PATH} AND NOT DEFINED PICO_SDK_PATH)
    message(FATAL_ERROR "PICO_SDK_PATH is not set. Point it at your pico-sdk checkout.")
endif()
if(NOT DEFINED ENV{PICO_EXTRAS_PATH} AND NOT DEFINED PICO_EXTRAS_PATH)
    message(FATAL_ERROR "PICO_EXTRAS_PATH is not set. Point it at your pico-extras checkout.")
endif()

# RP2350-only — we lean on the M33 FPU for the relativistic / lensing math.
set(PICO_BOARD pico2 CACHE STRING "Pico 2 / RP2350")

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(singularity C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

if (NOT PICO_PLATFORM STREQUAL "rp2350-arm-s" AND NOT PICO_PLATFORM STREQUAL "rp2350-riscv")
    message(FATAL_ERROR "13_Singularity is RP2350-only. Got PICO_PLATFORM=${PICO_PLATFORM}.")
endif()

add_executable(singularity
    main.c
    vga.c
    scene.c
    scene_scratch.c
    timeline.c
    audio_qoa.c
    music_qoa.S
    assets/_packed/assets.S
    assets/_packed/lens_lut.S

    effects/title.c
    effects/nebula.c
    effects/star.c
    effects/starfield.c
    effects/disk.c
    effects/lensing.c
    effects/spacetime.c
    effects/rebirth.c
)

target_include_directories(singularity PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed
)

# assets.S incbins each <name>.bin / <name>.pal from assets/_packed/.
set_source_files_properties(assets/_packed/assets.S PROPERTIES
    COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed"
    OBJECT_DEPENDS  "${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/title_bg.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/star_pano.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/disk_tex.bin"
)

# lens_lut.S incbins the offline-baked gravitational-lensing remap LUT.
# Re-run tools/make_lens_lut.py to regenerate; the dependency triggers a
# reassemble.
set_source_files_properties(assets/_packed/lens_lut.S PROPERTIES
    COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed"
    OBJECT_DEPENDS  "${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/lens_lut.bin"
)

# music_qoa.S incbins music.qoa (see IMPLEMENTATION.md §7 for the pipeline).
set_source_files_properties(music_qoa.S PROPERTIES
    COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}"
    OBJECT_DEPENDS  "${CMAKE_CURRENT_SOURCE_DIR}/music.qoa"
)

target_compile_definitions(singularity PRIVATE PICO_BUILD=1)

target_link_libraries(singularity
    pico_stdlib
    pico_scanvideo_dpi
    pico_multicore
    hardware_pwm
    hardware_dma
    hardware_interp      # SIO interpolators — reserved for the Mode-7 disk
)

target_compile_definitions(singularity PRIVATE
    PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=400
    PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=16
)

pico_enable_stdio_usb(singularity 1)
pico_enable_stdio_uart(singularity 0)

pico_add_extra_outputs(singularity)
