# Desktop test harness for QUICKSILVER (Project 15).
#
# Builds quicksilver.exe on Windows / MSYS2 UCRT64 with SDL2. Reuses every C
# source from the device build except main.c, vga.c, audio_qoa.c — those are
# replaced by main_host.c, vga_sdl.c, audio_sdl.c. The interpolator is provided
# by interp_emu.c (the software emulator; the device build uses the SIO
# peripheral instead and never compiles this file). assets.S + music_qoa.S are
# assembled by mingw GAS (their .incbin directives are target-agnostic).
#
# From an MSYS2 UCRT64 shell (or a shell with /ucrt64/bin on PATH):
#   make
#   ./quicksilver.exe
#   ./quicksilver.exe --screenshot-at 5000 --exit-after 5100
#
# Keys: ESC/Q quit, S screenshot, SPACE next scene, LEFT prev, R restart.

CC        := gcc
SDLCFLAGS := $(shell pkg-config --cflags sdl2)
SDLLIBS   := $(shell pkg-config --libs sdl2)

CFLAGS  := -std=gnu11 -O2 -g -Wall -Wno-unused-function \
           -I.. -I../assets/_packed \
           $(SDLCFLAGS) -DHOST_BUILD=1 -MMD -MP
ASFLAGS := -I.. -I../assets/_packed

ASM_OBJS = music_qoa.o assets.o

SHARED_C = ../scene.c ../scene_scratch.c ../timeline.c \
           ../interp_emu.c \
           ../effects/rotozoom.c \
           ../effects/mode7.c \
           ../effects/envmap3d.c \
           ../effects/chrome.c \
           ../effects/liquid.c \
           ../effects/tunnel.c \
           ../effects/title.c \
           ../effects/credits.c \
           ../effects/transition.c

HOST_C = main_host.c vga_sdl.c audio_sdl.c

C_OBJS = $(notdir $(SHARED_C:.c=.o)) $(HOST_C:.c=.o)
OBJS   = $(C_OBJS) $(ASM_OBJS)
TARGET = quicksilver.exe

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(SDLLIBS) -lm

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

scene.o: ../scene.c
	$(CC) $(CFLAGS) -c -o $@ $<
timeline.o: ../timeline.c
	$(CC) $(CFLAGS) -c -o $@ $<
scene_scratch.o: ../scene_scratch.c
	$(CC) $(CFLAGS) -c -o $@ $<
interp_emu.o: ../interp_emu.c
	$(CC) $(CFLAGS) -c -o $@ $<
rotozoom.o: ../effects/rotozoom.c
	$(CC) $(CFLAGS) -c -o $@ $<
mode7.o: ../effects/mode7.c
	$(CC) $(CFLAGS) -c -o $@ $<
envmap3d.o: ../effects/envmap3d.c
	$(CC) $(CFLAGS) -c -o $@ $<
chrome.o: ../effects/chrome.c
	$(CC) $(CFLAGS) -c -o $@ $<
liquid.o: ../effects/liquid.c
	$(CC) $(CFLAGS) -c -o $@ $<
tunnel.o: ../effects/tunnel.c
	$(CC) $(CFLAGS) -c -o $@ $<
title.o: ../effects/title.c
	$(CC) $(CFLAGS) -c -o $@ $<
credits.o: ../effects/credits.c
	$(CC) $(CFLAGS) -c -o $@ $<
transition.o: ../effects/transition.c
	$(CC) $(CFLAGS) -c -o $@ $<

# These .S files `.incbin` data blobs, so the blob is the real input — list it
# as a prerequisite or `make` won't rebuild when only the data changes (e.g. a
# new music.qoa relinks stale audio into the exe).
music_qoa.o: ../music_qoa.S ../music.qoa
	$(CC) $(ASFLAGS) -c -o $@ $<
assets.o: ../assets/_packed/assets.S $(wildcard ../assets/_packed/*.bin ../assets/_packed/*.pal)
	$(CC) $(ASFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET) $(C_OBJS:.o=.d)

.PHONY: all clean

# Auto-generated header dependencies (-MMD): rebuild a .o when any header it
# includes changes (e.g. qs_tunnel.h). Without this, header-only edits silently
# left tunnel.o stale and the host preview diverged from the device.
-include $(C_OBJS:.o=.d)
