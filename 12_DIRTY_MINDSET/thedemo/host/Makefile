# Desktop test harness for VOLTAGE.
# Builds thedemo.exe on Windows / MSYS2 UCRT64 with SDL2.

CC      := gcc
SDLCFLAGS := $(shell pkg-config --cflags sdl2)
SDLLIBS   := $(shell pkg-config --libs sdl2)

CFLAGS  := -std=gnu11 -O2 -g -Wall -Wno-unused-function \
           -I.. -I../assets/_packed \
           $(SDLCFLAGS) \
           -DHOST_BUILD=1

ASFLAGS := -I.. -I../assets/_packed

# Assembler sources
ASM_OBJS = music_qoa.o assets.o

# Shared C sources
SHARED_C = ../scene.c ../scene_scratch.c ../timeline.c \
           ../effects/cpc_boot.c \
           ../effects/plasma_chip.c \
           ../effects/text_matrix.c \
           ../effects/dirty_logo.c \
           ../effects/reaction_mind.c \
           ../effects/fractal_zoom.c \
           ../effects/greetings.c \
           ../effects/outro.c

# Host-only sources
HOST_C = main_host.c vga_sdl.c audio_sdl.c

C_OBJS = $(notdir $(SHARED_C:.c=.o)) $(HOST_C:.c=.o)
OBJS = $(C_OBJS) $(ASM_OBJS)

TARGET = thedemo.exe

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(SDLLIBS) -lm -mconsole

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

cpc_boot.o: ../effects/cpc_boot.c
	$(CC) $(CFLAGS) -c -o $@ $<
plasma_chip.o: ../effects/plasma_chip.c
	$(CC) $(CFLAGS) -c -o $@ $<
text_matrix.o: ../effects/text_matrix.c
	$(CC) $(CFLAGS) -c -o $@ $<
dirty_logo.o: ../effects/dirty_logo.c
	$(CC) $(CFLAGS) -c -o $@ $<
reaction_mind.o: ../effects/reaction_mind.c
	$(CC) $(CFLAGS) -c -o $@ $<
fractal_zoom.o: ../effects/fractal_zoom.c
	$(CC) $(CFLAGS) -c -o $@ $<
greetings.o: ../effects/greetings.c
	$(CC) $(CFLAGS) -c -o $@ $<
outro.o: ../effects/outro.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Asm sources
music_qoa.o: ../music_qoa.S ../music.qoa
	$(CC) $(ASFLAGS) -c -o $@ $<
assets.o: ../assets/_packed/assets.S
	$(CC) $(ASFLAGS) -c -o $@ $<

clean:
	Remove-Item -Force $(OBJS) $(TARGET) -ErrorAction SilentlyContinue

.PHONY: all clean
