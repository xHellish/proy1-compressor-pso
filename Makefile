CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic
SOURCE = main.c
GTK_CFLAGS = $(shell pkg-config --cflags gtk4)
GTK_LIBS = $(shell pkg-config --libs gtk4)

.PHONY: all clean

all: main

main: $(SOURCE)
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(SOURCE) -o main $(GTK_LIBS)

clean:
	rm -f main gutenberg_downloader ventana_gtk4