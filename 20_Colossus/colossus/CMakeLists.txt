cmake_minimum_required(VERSION 3.13)

# COLOSSUS builds two things from one set of sources: the RP2350 firmware and
# a host build (the SDL player and the no-SDL capture tool). The renderer is
# the same C in both, which is the only reason a host still and a photograph
# of the screen can be compared at all.
#
#   cmake -S colossus -B colossus/build_host  -DCOLOSSUS_HOST=ON
#   cmake -S colossus -B colossus/build_rp2350 -DPICO_SDK_PATH=... -DPICO_EXTRAS_PATH=...
#
# ../build.ps1 drives both. Under WSL, colossus/host/Makefile builds the
# capture tool with plain gcc and no cmake at all.
#
# Options:
#   COLOSSUS_STUB=ON   ignore render.cmake and use host/stub_demo.c, the
#                      platform's own stand-in renderer, for measuring the
#                      platform alone.
#   COLOSSUS_BALLAST=N link N bytes of unused .bss, to find the heap floor
#                      at which pico_scanvideo stops being able to boot.
#   COLOSSUS_SCANOUT_640=ON  scan out through vga_mode_640x480_60 and index
#                      the page with y>>1 (VESPER's arrangement, 480 line
#                      copies a frame) instead of the yscale=2 mode.
#   COLOSSUS_MATERIAL_TEST=ON  draw Phase's render_material_test() instead of
#                      the demo: PLANNING section 8's worst case, measured.

option(COLOSSUS_HOST "Build the host player and capture tool" OFF)
option(COLOSSUS_STUB "Use the platform's stub renderer" OFF)
option(COLOSSUS_SCANOUT_640 "Scan out at 640x480 with a doubled row index" OFF)
option(COLOSSUS_MATERIAL_TEST "Draw render_material_test() instead of the demo" OFF)
set(COLOSSUS_BALLAST "0" CACHE STRING "bytes of dead .bss, for the heap floor measurement")

# ---------------------------------------------------------------- renderer --
#
# Phase owns render.cmake and everything it names. The contract is that it
# sets COLOSSUS_RENDER_SOURCES to a list of .c files (relative paths are
# taken against this directory) and may add COLOSSUS_RENDER_INCLUDES and
# COLOSSUS_RENDER_DEFINES. Until it exists, or with -DCOLOSSUS_STUB=ON, the
# stub stands in and demo.h's contract is satisfied either way.

set(COLOSSUS_RENDER_SOURCES "")
set(COLOSSUS_RENDER_INCLUDES "")
set(COLOSSUS_RENDER_DEFINES "")
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/render.cmake AND NOT COLOSSUS_STUB)
    include(${CMAKE_CURRENT_SOURCE_DIR}/render.cmake)
    if(NOT COLOSSUS_RENDER_SOURCES AND RENDER_SOURCES)
        set(COLOSSUS_RENDER_SOURCES ${RENDER_SOURCES})   # tolerate the shorter name
    endif()
endif()
if(COLOSSUS_RENDER_SOURCES)
    set(COLOSSUS_DEMO_SOURCES ${COLOSSUS_RENDER_SOURCES})
    set(COLOSSUS_DEMO_KIND "render.cmake")
else()
    set(COLOSSUS_DEMO_SOURCES host/stub_demo.c)
    set(COLOSSUS_DEMO_KIND "stub")
endif()
message(STATUS "COLOSSUS renderer: ${COLOSSUS_DEMO_KIND}")

set(COLOSSUS_CORE_SOURCES song.c synth.c ${COLOSSUS_DEMO_SOURCES})

if(COLOSSUS_HOST)
# -------------------------------------------------------------------- host --
    project(colossus C)
    set(CMAKE_C_STANDARD 11)

    add_executable(capture tools/capture.c ${COLOSSUS_CORE_SOURCES})
    target_include_directories(capture PRIVATE . host ${COLOSSUS_RENDER_INCLUDES})
    target_compile_definitions(capture PRIVATE HOST_BUILD=1 ${COLOSSUS_RENDER_DEFINES})
    target_compile_options(capture PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter -fno-math-errno)
    if(NOT WIN32)
        target_link_libraries(capture PRIVATE m)
    endif()

    # SDL is optional on purpose: Phase builds this tree in WSL, where the
    # capture tool has to come up with nothing but gcc and libc.
    find_package(SDL2 CONFIG QUIET)
    if(SDL2_FOUND)
        add_executable(player host/player.c ${COLOSSUS_CORE_SOURCES})
        target_include_directories(player PRIVATE . host ${COLOSSUS_RENDER_INCLUDES})
        target_compile_definitions(player PRIVATE HOST_BUILD=1 ${COLOSSUS_RENDER_DEFINES})
        target_compile_options(player PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter -fno-math-errno)
        target_link_libraries(player PRIVATE SDL2::SDL2)
        if(NOT WIN32)
            target_link_libraries(player PRIVATE m)
        endif()
    else()
        message(STATUS "COLOSSUS: no SDL2, building the capture tool only")
    endif()
else()
# ------------------------------------------------------------------ RP2350 --
    set(PICO_BOARD pico2 CACHE STRING "Pico 2 / RP2350")
    set(PICO_PLATFORM rp2350-arm-s CACHE STRING "Cortex-M33 with FPU")
    if(NOT PICO_SDK_PATH)
        set(PICO_SDK_PATH "$ENV{PICO_SDK_PATH}")
    endif()
    if(NOT PICO_EXTRAS_PATH)
        set(PICO_EXTRAS_PATH "$ENV{PICO_EXTRAS_PATH}")
    endif()
    include(${PICO_SDK_PATH}/external/pico_sdk_import.cmake)
    include(${PICO_EXTRAS_PATH}/external/pico_extras_import.cmake)

    project(colossus C CXX ASM)
    set(CMAKE_C_STANDARD 11)
    pico_sdk_init()

    if(NOT PICO_PLATFORM STREQUAL "rp2350-arm-s")
        message(FATAL_ERROR "COLOSSUS is RP2350 Cortex-M33 only (got ${PICO_PLATFORM})")
    endif()

    add_executable(colossus main.c video.c audio_pwm.c ${COLOSSUS_CORE_SOURCES})
    target_include_directories(colossus PRIVATE . host ${COLOSSUS_RENDER_INCLUDES})

    target_compile_definitions(colossus PRIVATE
        PICO_BUILD=1
        CV_BALLAST=${COLOSSUS_BALLAST}
        # Route panic() to cv_panic() in main.c, which prints and then spins
        # with interrupts on instead of hitting a breakpoint. A breakpoint
        # with no debugger attached escalates to a HardFault that masks every
        # interrupt, killing USB -- an unreadable, unflashable board that
        # needs a human to unplug it. See main.c.
        PICO_PANIC_FUNCTION=cv_panic
        # 322 words carry one 640-pixel RAW_RUN scanline; 324 is the SDK's
        # rounding of that. Eight buffers is 10.4 KB of heap that the linker
        # never sees -- see main.c and tools/ledger_check.py.
        PICO_SCANVIDEO_MAX_SCANLINE_BUFFER_WORDS=324
        PICO_SCANVIDEO_SCANLINE_BUFFER_COUNT=8
        PICO_SCANVIDEO_MISSING_SCANLINE_COLOR=0
        PICO_STACK_SIZE=0x1000
        PICO_CORE1_STACK_SIZE=0x1000
        ${COLOSSUS_RENDER_DEFINES})
    if(COLOSSUS_SCANOUT_640)
        target_compile_definitions(colossus PRIVATE CV_SCANOUT_640=1)
    endif()
    if(COLOSSUS_MATERIAL_TEST)
        if(COLOSSUS_STUB)
            message(FATAL_ERROR "COLOSSUS_MATERIAL_TEST needs the renderer, not the stub")
        endif()
        target_compile_definitions(colossus PRIVATE CV_MATERIAL_TEST=1)
    endif()

    # -fno-math-errno lets the compiler use the FPU's own sqrt instead of a
    # libm call that also has to set errno. It does NOT license the
    # reassociation -ffast-math would, and it must not: the host WAV and the
    # device output are diffed by hash.
    target_compile_options(colossus PRIVATE -O2 -Wall -Wextra -Wno-unused-parameter -fno-math-errno)

    target_link_libraries(colossus
        pico_stdlib
        pico_multicore
        pico_scanvideo_dpi
        hardware_pwm
        hardware_dma
        hardware_interp)

    # The VGA board uses the UART pins. USB CDC is the only telemetry channel.
    pico_enable_stdio_usb(colossus 1)
    pico_enable_stdio_uart(colossus 0)
    pico_add_extra_outputs(colossus)
endif()
