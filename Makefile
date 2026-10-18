# Astrosis-C - lightweight spatial indexing & conjunction screening
#
# Linux / macOS / MSYS2:  make
# Run:                    ./astro            (window)
#                         ./astro --bench    (headless timing table)
# Debug (ASan/UBSan):     make debug

CC      ?= cc
CSTD    ?= -std=c11
WARN    ?= -Wall -Wextra
OPT     ?= -O2
CFLAGS  ?= $(CSTD) $(OPT) $(WARN)
LDFLAGS ?=

# SDL2 discovery: prefer pkg-config, fall back to sdl2-config.
SDL_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null || sdl2-config --cflags 2>/dev/null)
SDL_LIBS   := $(shell pkg-config --libs   sdl2 2>/dev/null || sdl2-config --libs   2>/dev/null)

# Optional: SDL2_ttf gives anti-aliased system-font text; the code auto-detects
# the header and falls back to the embedded bitmap font when it is absent.
TTF_CFLAGS := $(shell pkg-config --cflags SDL2_ttf 2>/dev/null)
TTF_LIBS   := $(shell pkg-config --libs   SDL2_ttf 2>/dev/null)

CFLAGS += -Iinclude $(SDL_CFLAGS) $(TTF_CFLAGS)
LDLIBS += $(SDL_LIBS) $(TTF_LIBS) -lm

# Explicit list: the SDL front-end (viz.c) is used on Linux/macOS. The native
# Win32 front-end (win32_ui.c) is built only by build.bat on Windows.
SRC := src/catalog.c src/physics.c src/kd_tree.c src/brute_force.c \
       src/scenes.c src/sim.c src/viz.c src/main.c
OBJ := $(patsubst src/%.c,obj/%.o,$(SRC))
BIN := astro

.PHONY: all debug run clean
all: $(BIN)

$(BIN): $(OBJ)
	$(CC) $(OBJ) -o $@ $(LDFLAGS) $(LDLIBS)

obj/%.o: src/%.c | obj
	$(CC) $(CFLAGS) -c $< -o $@

obj:
	mkdir -p obj

# Sanitizer build (valgrind is often unavailable; ASan/UBSan catch the same bugs)
debug: CFLAGS := $(CSTD) -O1 -g $(WARN) -fsanitize=address,undefined -fno-omit-frame-pointer -Iinclude $(SDL_CFLAGS)
debug: LDFLAGS += -fsanitize=address,undefined
debug: clean $(BIN)

run: $(BIN)
	./$(BIN)

clean:
	rm -rf obj $(BIN)
