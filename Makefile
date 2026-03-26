MODE     := dev
ifeq ($(MODE), dev)
	OFLAGS := -g -O0 -DDEBUG
else
	OFLAGS := -O3 -march=native
endif
CPPFLAGS := -I/usr/include/harfbuzz -I/usr/include/freetype2 -I/usr/include/libpng16 -I/usr/include/glib-2.0 -I/usr/lib/glib-2.0/include -I/usr/include/sysprof-6 -pthread
CFLAGS   := -std=c11 -pedantic -Wall $(OFLAGS)
LDFLAGS  := -lm -lX11 -lXrandr -lfreetype -lharfbuzz

CC = cc

TARGET_EXE := keymenu
BUILD_DIR := build
SRC_DIR   := src

all: build build-exe

build-exe: src/main.c
	$(CC) $(CFLAGS) $(CPPFLAGS) $(LDFLAGS) -o build/$(TARGET_EXE) $<

build:
	mkdir -p build

install: all
	strip build/$(TARGET_EXE)
	cp build/$(TARGET_EXE) ~/.local/bin

clean:
	rm -r build

.PHONY: all clean install build-exe
