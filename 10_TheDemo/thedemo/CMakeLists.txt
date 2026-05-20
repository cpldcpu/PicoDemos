cmake_minimum_required(VERSION 3.13)

# PICO_SDK_PATH and PICO_EXTRAS_PATH must be set in the environment (or
# passed via -D on the cmake command line). See README.md for setup.
if(NOT DEFINED ENV{PICO_SDK_PATH} AND NOT DEFINED PICO_SDK_PATH)
    message(FATAL_ERROR "PICO_SDK_PATH is not set. Point it at your pico-sdk checkout.")
endif()
if(NOT DEFINED ENV{PICO_EXTRAS_PATH} AND NOT DEFINED PICO_EXTRAS_PATH)
    message(FATAL_ERROR "PICO_EXTRAS_PATH is not set. Point it at your pico-extras checkout.")
endif()

# RP2350-only project (see ../README.md). The same code does NOT build for
# RP2040 — we depend on the M33 FPU for the SDF/raytracer scenes and have
# no plan to provide RP2040 fallbacks.
set(PICO_BOARD pico2 CACHE STRING "Pico 2 / RP2350")

include(pico_sdk_import.cmake)
include(pico_extras_import.cmake)

project(thedemo C CXX ASM)
set(CMAKE_C_STANDARD 11)

pico_sdk_init()

if (NOT PICO_PLATFORM STREQUAL "rp2350-arm-s" AND NOT PICO_PLATFORM STREQUAL "rp2350-riscv")
    message(FATAL_ERROR "20_TheDemo is RP2350-only. Got PICO_PLATFORM=${PICO_PLATFORM}.")
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

    effects/voxel_landscape.c
    effects/title.c
    effects/fluid_portraits.c
    effects/spheres.c
    effects/greetz.c
    effects/reaction_diffusion.c
    effects/tunnel_particles.c
    effects/rotozoom_credits.c
)

target_include_directories(thedemo PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed
)

# assets.S incbins each <name>.bin / <name>.pal from assets/_packed/.
# Tell the assembler where to find them, and declare deps so re-running
# pack_assets.py triggers a rebuild.
set_source_files_properties(assets/_packed/assets.S PROPERTIES
    COMPILE_OPTIONS "-I${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed"
    OBJECT_DEPENDS  "${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/voxel_color.bin;${CMAKE_CURRENT_SOURCE_DIR}/assets/_packed/voxel_height.bin"
)

# music_qoa.S incbins music.qoa from this directory. Generate it with:
#   ffmpeg -i music.mp3 -ac 1 -ar 22050 -f s16le music.raw
#   ./tools/qoaconv_s16.exe music.raw music.qoa 22050 1
# Tell the assembler where to find it, and declare the dependency so
# re-encoding the soundtrack triggers reassembly.
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

# scanvideo buffer pool. 200 words/line is enough for 320-wide composable
# scanlines and gives headroom for 640-wide later (162 words for 320, 324
# words for 640 — bump this when we wire 640-mode).
target_compile_definitions(thedemo PRIVATE
    PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=400
    PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=16
)

# USB CDC stdio for printf. UART disabled — we may need GP0/GP1 for I2S
# audio later.
pico_enable_stdio_usb(thedemo 1)
pico_enable_stdio_uart(thedemo 0)

pico_add_extra_outputs(thedemo)
