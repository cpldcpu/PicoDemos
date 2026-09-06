# Desktop harness for SUSTAIN (Project 16).
#
# Builds sustain.exe on Windows / MSYS2 UCRT64 with SDL2. Reuses the device
# sources except main.c / vga.c / audio_qoa.c, which are replaced by
# main_host.c / vga_sdl.c / audio_sdl.c.
#
#   make
#   ./sustain.exe
#   ./sustain.exe --screenshot-at 9000 --exit-after 9100
#   ./sustain.exe --rawpipe | python ../tools/cut_detect.py      # the rule-1 audit
#
# Keys: ESC/Q quit, S screenshot, SPACE next arc node, LEFT prev, R restart.

CC        := gcc
SDLCFLAGS := $(shell pkg-config --cflags sdl2)
SDLLIBS   := $(shell pkg-config --libs sdl2)

CFLAGS  := -std=gnu11 -O2 -g -Wall -Wno-unused-function \
           -I.. -I../assets/_packed \
           $(SDLCFLAGS) -DHOST_BUILD=1 -MMD -MP
ASFLAGS := -I.. -I../assets/_packed

ASM_OBJS = music_qoa.o assets.o

SHARED_C = ../world.c ../arc.c ../render.c ../field_scratch.c ../motes.c ../overlay.c ../mip.c \
           ../interp_emu.c \
           ../fields/f_terrain.c \
           ../fields/f_cave.c \
           ../fields/f_monolith.c \
           ../fields/sky_common.c

HOST_C = main_host.c vga_sdl.c audio_sdl.c

C_OBJS = $(notdir $(SHARED_C:.c=.o)) $(HOST_C:.c=.o)
OBJS   = $(C_OBJS) $(ASM_OBJS)
TARGET = sustain.exe

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(SDLLIBS) -lm

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

world.o: ../world.c
	$(CC) $(CFLAGS) -c -o $@ $<
arc.o: ../arc.c
	$(CC) $(CFLAGS) -c -o $@ $<
render.o: ../render.c
	$(CC) $(CFLAGS) -c -o $@ $<
motes.o: ../motes.c
	$(CC) $(CFLAGS) -c -o $@ $<
overlay.o: ../overlay.c
	$(CC) $(CFLAGS) -c -o $@ $<
mip.o: ../mip.c
	$(CC) $(CFLAGS) -c -o $@ $<
field_scratch.o: ../field_scratch.c
	$(CC) $(CFLAGS) -c -o $@ $<
interp_emu.o: ../interp_emu.c
	$(CC) $(CFLAGS) -c -o $@ $<
f_terrain.o: ../fields/f_terrain.c
	$(CC) $(CFLAGS) -c -o $@ $<
f_cave.o: ../fields/f_cave.c
	$(CC) $(CFLAGS) -c -o $@ $<
f_monolith.o: ../fields/f_monolith.c
	$(CC) $(CFLAGS) -c -o $@ $<
sky_common.o: ../fields/sky_common.c
	$(CC) $(CFLAGS) -c -o $@ $<

# .S incbins a data blob, so the blob is the real input — list it as a
# prerequisite or a new music.qoa / repacked asset silently relinks stale data.
music_qoa.o: ../music_qoa.S ../music.qoa
	$(CC) $(ASFLAGS) -c -o $@ $<
assets.o: ../assets/_packed/assets.S $(wildcard ../assets/_packed/*.bin)
	$(CC) $(ASFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET) $(C_OBJS:.o=.d)

.PHONY: all clean

-include $(C_OBJS:.o=.d)
