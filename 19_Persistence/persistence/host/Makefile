# Desktop player for PERSISTENCE (Project 19).
#
#   make
#   ./persistence.exe                 # watch it, with sound
#   ./persistence.exe --start 67      # seek: this demo stores nothing
#   ./persistence.exe --wav music.wav # the soundtrack alone
#
# -O2, not -O3 -ffast-math: kernels are integer by design and the host render
# is diffed against the device by hash. The interpolator kernels go through
# QUICKSILVER's bit-exact emulator (interp_emu.c), which is compiled here and
# never by CMake.

CC        := gcc
SDLCFLAGS := $(shell pkg-config --cflags sdl2)
SDLLIBS   := $(filter-out -mwindows,$(shell pkg-config --libs sdl2))

CFLAGS := -std=gnu11 -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-function \
          -I.. -DHOST_BUILD=1 -MMD -MP $(SDLCFLAGS)

SHARED_C := $(wildcard ../fx_*.c) ../beam.c ../timeline.c ../tables.c ../synth.c ../song.c \
            ../interp_emu.c ../s3d.c ../mesh.c ../affine.c ../text.c
HOST_C   := main_host.c

OBJS   := $(notdir $(SHARED_C:.c=.o)) $(HOST_C:.c=.o)
TARGET := persistence.exe

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(SDLLIBS) -lm

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: ../%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o *.d $(TARGET)

.PHONY: all clean
-include $(OBJS:.o=.d)
