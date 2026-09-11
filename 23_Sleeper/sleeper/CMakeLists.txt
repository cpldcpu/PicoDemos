cmake_minimum_required(VERSION 3.13)
option(SLEEPER_HOST "Build the SDL2 desktop player" OFF)
option(SLEEPER_DMA "DMA background transfer" ON)
option(SLEEPER_INTERP "Core-0 SIO texture addressing and palette blending" ON)
# Measurement only, never a release build: audio_pump() keeps the ring, the
# DMA polling and the chunking but renders silence instead of calling the
# synth, which prices the pump machinery on its own. (Overscan)
option(SLEEPER_SILENT_PUMP "Run audio_pump() with no DSP, for timing only" OFF)

# The synth is Phosphor's and is not here yet. Until synth.c exists the build
# links synth_stub.c, which honours the contract with silence; the moment
# synth.c appears beside it nothing else changes. That is the whole of the
# swap the round-one brief asked for.
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/synth.c)
    set(SLEEPER_SYNTH_SOURCE synth.c)
    set(SLEEPER_STUB 0)
else()
    set(SLEEPER_SYNTH_SOURCE synth_stub.c)
    set(SLEEPER_STUB 1)
endif()
message(STATUS "SLEEPER synth source: ${SLEEPER_SYNTH_SOURCE}")

set(SLEEPER_RENDER render.c lights.c board.c sky.c assets.c world_side.c world_ahead.c world_tunnel.c world_dream.c song.c)

add_compile_definitions(SLEEPER_DMA=$<BOOL:${SLEEPER_DMA}> SLEEPER_INTERP=$<BOOL:${SLEEPER_INTERP}>
    SLEEPER_SILENT_PUMP=$<BOOL:${SLEEPER_SILENT_PUMP}> SLEEPER_STUB_SYNTH=${SLEEPER_STUB})

if(SLEEPER_HOST)
    project(sleeper C)
    set(CMAKE_C_STANDARD 11)
    find_package(SDL2 CONFIG REQUIRED)
    add_executable(sleeper host/main.c ${SLEEPER_RENDER} ${SLEEPER_SYNTH_SOURCE})
    target_include_directories(sleeper PRIVATE .)
    target_compile_options(sleeper PRIVATE -O2 -Wall -Wextra -Wno-misleading-indentation)
    target_link_libraries(sleeper PRIVATE SDL2::SDL2 m)
    # -ftrapv across the whole check build again, synth.c and song.c
    # included. Round one had to confine it to an object library because two
    # Q16 products in control_tick() overflowed a signed int and trapped;
    # Phosphor put both through int64_t, so the guard covers every line of
    # the production once more and a trap is a defect rather than a known
    # exception. (Overscan)
    add_executable(sleeper_check tools/check.c ${SLEEPER_RENDER} ${SLEEPER_SYNTH_SOURCE})
    target_include_directories(sleeper_check PRIVATE .)
    target_compile_options(sleeper_check PRIVATE -O2 -Wall -Wextra -Wno-misleading-indentation -ftrapv)
    target_link_libraries(sleeper_check PRIVATE m)
    # Phosphor's harness: --cuts and --boards feed the referees, --hashes
    # feeds the device diff. Not part of either shipping target.
    add_executable(song_harness tools/song_harness.c song.c ${SLEEPER_SYNTH_SOURCE})
    target_include_directories(song_harness PRIVATE .)
    target_compile_options(song_harness PRIVATE -O2 -Wall -Wextra)
    target_link_libraries(song_harness PRIVATE m)
    # The lights still the round-one brief asks for before anything is built
    # on lights.c: the production's own light_draw(), not an illustration.
    add_executable(light_sheet tools/light_sheet.c lights.c)
    target_include_directories(light_sheet PRIVATE .)
    target_compile_options(light_sheet PRIVATE -O2 -Wall -Wextra)
    target_link_libraries(light_sheet PRIVATE m)

    enable_testing()
    add_test(NAME whole_production COMMAND sleeper_check)
else()
    set(PICO_BOARD pico2 CACHE STRING "RP2350 board")
    set(PICO_PLATFORM rp2350-arm-s CACHE STRING "Cortex-M33 with FPU")
    if(NOT PICO_SDK_PATH)
        set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
    endif()
    if(NOT PICO_EXTRAS_PATH)
        set(PICO_EXTRAS_PATH "$ENV{PICO_EXTRAS_PATH}")
    endif()
    include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
    include(${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)
    project(sleeper C CXX ASM)
    set(CMAKE_C_STANDARD 11)
    pico_sdk_init()
    if(NOT PICO_PLATFORM STREQUAL "rp2350-arm-s")
        message(FATAL_ERROR "SLEEPER requires RP2350 Cortex-M33 (rp2350-arm-s)")
    endif()
    add_executable(sleeper main.c video.c audio_pwm.c ${SLEEPER_RENDER} ${SLEEPER_SYNTH_SOURCE})
    target_include_directories(sleeper PRIVATE .)
    target_compile_definitions(sleeper PRIVATE PICO_BUILD=1
        PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=324
        PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=12
        PICO_STACK_SIZE=0x1000 PICO_CORE1_STACK_SIZE=0x1000
        # A panic that keeps USB alive (main.c): the stock one ends in a
        # breakpoint, which without a debugger becomes a HardFault with every
        # interrupt masked, and the board then needs a hand on the cable.
        PICO_PANIC_FUNCTION=sleeper_panic)
    target_compile_options(sleeper PRIVATE -O2 -Wall -Wextra -Wno-misleading-indentation)
    target_link_libraries(sleeper pico_stdlib pico_multicore pico_scanvideo_dpi hardware_pwm hardware_dma m)
    if(SLEEPER_INTERP)
        target_link_libraries(sleeper hardware_interp)
    endif()
    pico_enable_stdio_usb(sleeper 1)
    # UART defaults collide with VGA. USB CDC is the sole telemetry channel.
    pico_enable_stdio_uart(sleeper 0)
    pico_add_extra_outputs(sleeper)
endif()
