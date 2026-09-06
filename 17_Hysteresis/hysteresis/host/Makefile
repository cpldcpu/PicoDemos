# Desktop harness for HYSTERESIS (Project 17).
#
#   make
#   ./hysteresis.exe                        # watch it
#   ./hysteresis.exe --headless --frames 600 --energy
#   python ../tools/no_keyframes.py         # the referee
#
# NOTE -O2, not -O3 -ffast-math. The simulation is integer-only on purpose
# (field.h): referee test 1 diffs a host render against a device render byte
# for byte, and that only means something if neither compiler is licensed to
# reassociate arithmetic. Speed here is irrelevant anyway — the number that
# matters is measured on the device.

CC        := gcc
SDLCFLAGS := $(shell pkg-config --cflags sdl2)
SDLLIBS   := $(shell pkg-config --libs sdl2)

BASECFLAGS := -std=gnu11 -O2 -g -Wall -Wextra -Wno-unused-parameter \
              -I.. -DHOST_BUILD=1 -MMD -MP
CFLAGS := $(BASECFLAGS) $(SDLCFLAGS)

SHARED_C = ../field.c ../sim.c ../palette.c ../rd.c ../synth.c ../score.c
HOST_C   = main_host.c vga_sdl.c audio_sdl.c

OBJS   = $(notdir $(SHARED_C:.c=.o)) $(HOST_C:.c=.o)
TARGET = hysteresis.exe

# The WAV renderer links the synth and the score and nothing else -- no SDL, no
# field. Auditioning the music must not require a working display, and it must
# be fast enough to do twenty times in an evening.
WAV_OBJS = synthwav.o synth.o score.o
WAV      = synthwav.exe

all: $(TARGET) $(WAV)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(SDLLIBS) -lm

$(WAV): $(WAV_OBJS)
	$(CC) -o $@ $^ -lm

# NOT $(CFLAGS): SDL's cflags carry -Dmain=SDL_main, which renames this tool's
# entry point out from under it.
synthwav.o: synthwav.c
	$(CC) $(BASECFLAGS) -c -o $@ $<

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

field.o: ../field.c
	$(CC) $(CFLAGS) -c -o $@ $<
sim.o: ../sim.c
	$(CC) $(CFLAGS) -c -o $@ $<
palette.o: ../palette.c
	$(CC) $(CFLAGS) -c -o $@ $<
rd.o: ../rd.c
	$(CC) $(CFLAGS) -c -o $@ $<
synth.o: ../synth.c
	$(CC) $(CFLAGS) -c -o $@ $<
score.o: ../score.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(WAV_OBJS) $(TARGET) $(WAV) $(OBJS:.o=.d) $(WAV_OBJS:.o=.d)

.PHONY: all clean synthwav
synthwav: $(WAV)

-include $(OBJS:.o=.d)
