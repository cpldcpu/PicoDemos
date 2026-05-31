# Desktop test harness for ORIGAMI (Project 14).
#
# Builds origami.exe on Windows / MSYS2 UCRT64 with SDL2. Reuses every C
# source from the device build except main.c, vga.c, audio_qoa.c — those are
# replaced by main_host.c, vga_sdl.c, audio_sdl.c. music_qoa.S is assembled
# by mingw GAS (its .incbin is target-agnostic).
#
# From an MSYS2 UCRT64 shell (or PowerShell with /ucrt64/bin on PATH):
#   make
#   ./origami.exe                       # interactive, with audio
#   ./origami.exe --start-ms 80000 --screenshot-at 80000 --exit-after 80100
#
# Keys: ESC/Q quit, S screenshot, SPACE next scene, LEFT prev, R restart.

CC        := gcc
SDLCFLAGS := $(shell pkg-config --cflags sdl2)
SDLLIBS   := $(shell pkg-config --libs sdl2)

CFLAGS  := -std=gnu11 -O2 -g -Wall -Wno-unused-function \
           -I.. -I../effects \
           $(SDLCFLAGS) -DHOST_BUILD=1
ASFLAGS := -I..

ASM_OBJS = music_qoa.o

SHARED_C = ../scene.c ../scene_scratch.c ../timeline.c \
           ../effects/poly3d.c \
           ../effects/title.c \
           ../effects/plane.c \
           ../effects/crane.c \
           ../effects/city.c \
           ../effects/miura.c \
           ../effects/credits.c

HOST_C = main_host.c vga_sdl.c audio_sdl.c

C_OBJS = $(notdir $(SHARED_C:.c=.o)) $(HOST_C:.c=.o)
OBJS   = $(C_OBJS) $(ASM_OBJS)
TARGET = origami.exe

# `all` must be the FIRST target so it is the default goal.
all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(SDLLIBS) -lm

# Header dependency tracking: rebuild every object when ANY shared header
# changes (poly3d.h, origami_fx.h, vga.h, scene*.h, rgb565.h, font8x8.h …).
# Without this, edits to header-only helpers like origami_fx.h would not
# trigger a recompile and the host build would silently go stale. (Placed
# AFTER `all` so it doesn't become the default goal.)
HEADERS = $(wildcard ../*.h) $(wildcard ../effects/*.h)
$(C_OBJS): $(HEADERS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

scene.o: ../scene.c
	$(CC) $(CFLAGS) -c -o $@ $<
timeline.o: ../timeline.c
	$(CC) $(CFLAGS) -c -o $@ $<
scene_scratch.o: ../scene_scratch.c
	$(CC) $(CFLAGS) -c -o $@ $<
poly3d.o: ../effects/poly3d.c
	$(CC) $(CFLAGS) -c -o $@ $<
title.o: ../effects/title.c
	$(CC) $(CFLAGS) -c -o $@ $<
plane.o: ../effects/plane.c
	$(CC) $(CFLAGS) -c -o $@ $<
crane.o: ../effects/crane.c
	$(CC) $(CFLAGS) -c -o $@ $<
city.o: ../effects/city.c
	$(CC) $(CFLAGS) -c -o $@ $<
miura.o: ../effects/miura.c
	$(CC) $(CFLAGS) -c -o $@ $<
credits.o: ../effects/credits.c
	$(CC) $(CFLAGS) -c -o $@ $<

music_qoa.o: ../music_qoa.S
	$(CC) $(ASFLAGS) -c -o $@ $<

# Portable clean: try both POSIX `rm` (MSYS/bash shell) and Windows `del`
# (cmd shell). The leading `-` ignores whichever one isn't available.
# Note: thanks to the header-dependency rule above, a plain `make` already
# rebuilds correctly after any source/header edit — `clean` is rarely needed.
clean:
	-rm -f $(OBJS) $(TARGET)
	-del /q *.o origami.exe 2>nul

.PHONY: all clean
