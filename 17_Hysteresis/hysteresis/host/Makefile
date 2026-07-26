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

CFLAGS := -std=gnu11 -O2 -g -Wall -Wextra -Wno-unused-parameter \
          -I.. $(SDLCFLAGS) -DHOST_BUILD=1 -MMD -MP

SHARED_C = ../field.c ../sim.c ../palette.c ../rd.c
HOST_C   = main_host.c vga_sdl.c

OBJS   = $(notdir $(SHARED_C:.c=.o)) $(HOST_C:.c=.o)
TARGET = hysteresis.exe

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(SDLLIBS) -lm

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

clean:
	rm -f $(OBJS) $(TARGET) $(OBJS:.o=.d)

.PHONY: all clean

-include $(OBJS:.o=.d)
