# The host build with nothing but gcc and make.
#
# ../build.ps1 uses cmake on Windows; this exists because Phase builds in WSL,
# where the capture tool has to come up with no SDL, no cmake and no image
# library. Same sources, same flags, same renderer.
#
#   make                 the capture tool  -> build/capture
#   make stub=1          force the platform's stub renderer
#   make player          the SDL2 player (needs sdl2-config on PATH)
#   make wav             build/score.wav, the whole 5:07
#   make hashes          build/hashes.txt, referee 2's host side
#   make stills          build/stills/, one PNG per phrase downbeat
#   make clean

ROOT   := $(patsubst %/,%,$(dir $(abspath $(lastword $(MAKEFILE_LIST)))))/..
BUILD  := build
CC     ?= gcc
CFLAGS ?= -O2 -std=gnu11 -Wall -Wextra -Wno-unused-parameter -fno-math-errno
CPPFLAGS += -DHOST_BUILD=1 -I$(ROOT) -I$(ROOT)/host
LDLIBS ?= -lm

# Phase's renderer if it is in the tree, the platform's stub otherwise. The
# source list is read out of render.cmake rather than globbed, because that
# file is the one place it is declared and it deliberately excludes files
# that must not be linked (render_host.c, render_checks.c).
RENDER := $(patsubst %,$(ROOT)/%,$(shell sed -n 's@^[[:space:]]*$${CMAKE_CURRENT_LIST_DIR}/\([A-Za-z0-9_/.-]*\.c\)[[:space:]]*$$@\1@p' $(ROOT)/render.cmake 2>/dev/null))
ifeq ($(stub),1)
RENDER :=
endif
ifeq ($(strip $(RENDER)),)
RENDER := $(ROOT)/host/stub_demo.c
endif

CORE := $(ROOT)/song.c $(ROOT)/synth.c $(RENDER)

.PHONY: all player wav hashes stills clean
all: $(BUILD)/capture

$(BUILD):
	mkdir -p $(BUILD)

.PHONY: sources
sources:
	@echo $(RENDER)

$(BUILD)/capture: $(ROOT)/tools/capture.c $(CORE) | $(BUILD)
	$(CC) $(CFLAGS) $(CPPFLAGS) -o $@ $^ $(LDLIBS)

player: $(BUILD)/player
$(BUILD)/player: $(ROOT)/host/player.c $(CORE) | $(BUILD)
	$(CC) $(CFLAGS) $(CPPFLAGS) $(shell sdl2-config --cflags) -o $@ $^ $(shell sdl2-config --libs) $(LDLIBS)

wav: $(BUILD)/capture
	$(BUILD)/capture --wav $(BUILD)/score.wav

hashes: $(BUILD)/capture
	$(BUILD)/capture --hashes $(BUILD)/hashes.txt

stills: $(BUILD)/capture
	$(BUILD)/capture --out $(BUILD)/stills --phrases

clean:
	rm -rf $(BUILD)
