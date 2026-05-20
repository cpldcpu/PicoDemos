# Desktop test harness for 20_TheDemo.
#
# Builds thedemo.exe on Windows / MSYS2 UCRT64 with SDL2. Reuses every C
# source from the device build except main.c, vga.c, audio_qoa.c —
# those are replaced by main_host.c, vga_sdl.c, audio_sdl.c. The same
# assets.S and music_qoa.S are assembled by mingw GAS (their .incbin
# directives + section attributes are target-agnostic).
#
# Run inside an MSYS2 UCRT64 shell. Or, on Windows, from PowerShell with
# the UCRT64 toolchain on PATH:
#   $env:PATH = "$env:MSYS2_ROOT\ucrt64\bin;$env:MSYS2_ROOT\usr\bin;" + $env:PATH
#   make
#   .\thedemo.exe
#
# Keys: ESC/Q quit, S screenshot to screenshot_NNN.bmp, SPACE pause.

CC      := gcc
SDLCFLAGS := $(shell pkg-config --cflags sdl2)
SDLLIBS   := $(shell pkg-config --libs sdl2)

CFLAGS  := -std=gnu11 -O2 -g -Wall -Wno-unused-function \
           -I.. -I../assets/_packed \
           $(SDLCFLAGS) \
           -DHOST_BUILD=1

ASFLAGS := -I.. -I../assets/_packed

# Assembler sources need an explicit rule because they live in different
# subdirectories and need different -I paths.
ASM_OBJS = music_qoa.o assets.o

# Shared C sources (relative to host/ — same files the Pico build uses).
SHARED_C = ../scene.c ../scene_scratch.c ../timeline.c \
           ../effects/voxel_landscape.c \
           ../effects/title.c \
           ../effects/fluid_portraits.c \
           ../effects/spheres.c \
           ../effects/greetz.c \
           ../effects/reaction_diffusion.c \
           ../effects/tunnel_particles.c \
           ../effects/rotozoom_credits.c

# Host-only sources.
HOST_C = main_host.c vga_sdl.c audio_sdl.c

C_OBJS = $(notdir $(SHARED_C:.c=.o)) $(HOST_C:.c=.o)

OBJS = $(C_OBJS) $(ASM_OBJS)

TARGET = thedemo.exe

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(SDLLIBS) -lm

# Host-only .c files
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Shared .c files in parent dirs
scene.o: ../scene.c
	$(CC) $(CFLAGS) -c -o $@ $<
timeline.o: ../timeline.c
	$(CC) $(CFLAGS) -c -o $@ $<
scene_scratch.o: ../scene_scratch.c
	$(CC) $(CFLAGS) -c -o $@ $<
voxel_landscape.o: ../effects/voxel_landscape.c
	$(CC) $(CFLAGS) -c -o $@ $<
title.o: ../effects/title.c
	$(CC) $(CFLAGS) -c -o $@ $<
fluid_portraits.o: ../effects/fluid_portraits.c
	$(CC) $(CFLAGS) -c -o $@ $<
spheres.o: ../effects/spheres.c
	$(CC) $(CFLAGS) -c -o $@ $<
greetz.o: ../effects/greetz.c
	$(CC) $(CFLAGS) -c -o $@ $<
reaction_diffusion.o: ../effects/reaction_diffusion.c
	$(CC) $(CFLAGS) -c -o $@ $<
tunnel_particles.o: ../effects/tunnel_particles.c
	$(CC) $(CFLAGS) -c -o $@ $<
rotozoom_credits.o: ../effects/rotozoom_credits.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Asm sources with their own -I path (the incbin search path).
music_qoa.o: ../music_qoa.S
	$(CC) $(ASFLAGS) -c -o $@ $<
assets.o: ../assets/_packed/assets.S
	$(CC) $(ASFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
