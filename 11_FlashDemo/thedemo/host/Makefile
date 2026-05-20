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
           ../effects/spark_gap.c \
           ../effects/plasma_core.c \
           ../effects/ray_volt.c \
           ../effects/vector_strike.c \
           ../effects/spark_generator.c \
           ../effects/canyon_flight.c \
           ../effects/julia_shockwave.c \
           ../effects/voltage_arc.c

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

spark_gap.o: ../effects/spark_gap.c
	$(CC) $(CFLAGS) -c -o $@ $<
plasma_core.o: ../effects/plasma_core.c
	$(CC) $(CFLAGS) -c -o $@ $<
ray_volt.o: ../effects/ray_volt.c
	$(CC) $(CFLAGS) -c -o $@ $<
vector_strike.o: ../effects/vector_strike.c
	$(CC) $(CFLAGS) -c -o $@ $<
spark_generator.o: ../effects/spark_generator.c
	$(CC) $(CFLAGS) -c -o $@ $<
canyon_flight.o: ../effects/canyon_flight.c
	$(CC) $(CFLAGS) -c -o $@ $<
julia_shockwave.o: ../effects/julia_shockwave.c
	$(CC) $(CFLAGS) -c -o $@ $<
voltage_arc.o: ../effects/voltage_arc.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Asm sources
music_qoa.o: ../music_qoa.S ../music.qoa
	$(CC) $(ASFLAGS) -c -o $@ $<
assets.o: ../assets/_packed/assets.S
	$(CC) $(ASFLAGS) -c -o $@ $<

clean:
	Remove-Item -Force $(OBJS) $(TARGET) -ErrorAction SilentlyContinue

.PHONY: all clean
