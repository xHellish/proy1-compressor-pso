CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic
LIBS = $(shell pkg-config --cflags --libs libcurl)
SOURCE = gutenberg_downloader.c
TARGET = gutenberg_downloader

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCE)
	$(CC) $(CFLAGS) $(SOURCE) -o $(TARGET) $(LIBS)

clean:
	rm -f $(TARGET)