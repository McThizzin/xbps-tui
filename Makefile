CC      ?= gcc
CFLAGS  ?= -O2 -Wall -Wextra
CPPFLAGS += -Isrc
LDLIBS   = -lncurses -lutil

SRCS := $(wildcard src/*.c)
OBJS := $(patsubst src/%.c,build/%.o,$(SRCS))

xbps-tui: $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS) $(LDLIBS)

build/%.o: src/%.c | build
	$(CC) $(CFLAGS) $(CPPFLAGS) -c -o $@ $<

build:
	mkdir -p build

clean:
	rm -rf build xbps-tui

.PHONY: clean