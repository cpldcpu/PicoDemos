# Desktop test harness for SINGULARITY (Project 13).
#
# Builds singularity.exe on Windows / MSYS2 UCRT64 with SDL2. Reuses every
# C source from the device build except main.c, vga.c, audio_qoa.c — those
# are replaced by main_host.c, vga_sdl.c, audio_sdl.c. The same assets.S,
# lens_lut.S and music_qoa.S are assembled by mingw GAS (their .incbin
# directives are target-agnostic).
#
# From an MSYS2 UCRT64 shell (or PowerShell with /ucrt64/bin on PATH):
#   make
#   ./singularity.exe                       # interactive, with audio
#   ./singularity.exe --start-ms 200000 --screenshot-at 200000 --exit-after 200100
#
# Keys: ESC/Q quit, S screenshot, SPACE next scene, LEFT prev, R restart.

CC        := gcc
SDLCFLAGS := $(shell pkg-config --cflags sdl2)
SDLLIBS   := $(shell pkg-config --libs sdl2)

CFLAGS  := -std=gnu11 -O2 -g -Wall -Wno-unused-function \
           -I.. -I../assets/_packed \
           $(SDLCFLAGS) -DHOST_BUILD=1
ASFLAGS := -I.. -I../assets/_packed

ASM_OBJS = music_qoa.o assets.o lens_lut.o

SHARED_C = ../scene.c ../scene_scratch.c ../timeline.c \
           ../effects/title.c \
           ../effects/nebula.c \
           ../effects/star.c \
           ../effects/starfield.c \
           ../effects/disk.c \
           ../effects/lensing.c \
           ../effects/spacetime.c \
           ../effects/rebirth.c

HOST_C = main_host.c vga_sdl.c audio_sdl.c

C_OBJS = $(notdir $(SHARED_C:.c=.o)) $(HOST_C:.c=.o)
OBJS   = $(C_OBJS) $(ASM_OBJS)
TARGET = singularity.exe

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
title.o: ../effects/title.c
	$(CC) $(CFLAGS) -c -o $@ $<
nebula.o: ../effects/nebula.c
	$(CC) $(CFLAGS) -c -o $@ $<
star.o: ../effects/star.c
	$(CC) $(CFLAGS) -c -o $@ $<
starfield.o: ../effects/starfield.c
	$(CC) $(CFLAGS) -c -o $@ $<
disk.o: ../effects/disk.c
	$(CC) $(CFLAGS) -c -o $@ $<
lensing.o: ../effects/lensing.c
	$(CC) $(CFLAGS) -c -o $@ $<
spacetime.o: ../effects/spacetime.c
	$(CC) $(CFLAGS) -c -o $@ $<
rebirth.o: ../effects/rebirth.c
	$(CC) $(CFLAGS) -c -o $@ $<

music_qoa.o: ../music_qoa.S
	$(CC) $(ASFLAGS) -c -o $@ $<
assets.o: ../assets/_packed/assets.S
	$(CC) $(ASFLAGS) -c -o $@ $<
lens_lut.o: ../assets/_packed/lens_lut.S
	$(CC) $(ASFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean
