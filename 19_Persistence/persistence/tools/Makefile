# The referee. Deliberately NOT built by host/Makefile: it links no SDL, so it
# runs anywhere and at full speed, and a broken display cannot make it pass.
#
#   make && ./audit.exe
#   make && ./audit.exe --quick        # 1/13 of the frames, 20 s of audio

CC     := gcc
CFLAGS := -std=gnu11 -O2 -g -Wall -Wextra -Wno-unused-parameter -Wno-unused-function \
          -I.. -DHOST_BUILD=1 -MMD -MP

SRC := audit.c \
       $(wildcard ../fx_*.c) \
       ../beam.c ../timeline.c ../tables.c ../synth.c ../song.c \
       ../s3d.c ../mesh.c ../affine.c ../text.c ../interp_emu.c

OBJ := $(notdir $(SRC:.c=.o))

all: audit.exe

audit.exe: $(OBJ)
	$(CC) -o $@ $^ -lm

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: ../%.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f *.o *.d audit.exe

.PHONY: all clean
-include $(OBJ:.o=.d)
