CC ?= cc
CFLAGS ?= -std=c99 -O2 -Wall -Wextra -pedantic
INCLUDES = -Iinclude
NCURSES_LIBS ?= $(shell pkg-config --libs ncursesw 2>/dev/null || echo -lncursesw)
SRC = src/main.c src/cli.c src/filesystem.c src/audio.c src/sanitize.c src/tags.c src/organizer.c src/simulate.c src/export.c src/tui.c src/downloader.c

all: cartag

cartag: $(SRC)
	$(CC) $(CFLAGS) $(INCLUDES) -o $@ $(SRC) $(NCURSES_LIBS)

clean:
	rm -f cartag

.PHONY: all clean
