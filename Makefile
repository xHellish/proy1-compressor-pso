CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic
SOURCE = main.c gtk_ui.c
COMPRESSORS = compresor_normal compresor_fork compresor_pthread
DECOMPRESSORS = descompresor_normal descompresor_fork descompresor_pthread
GTK_CFLAGS = $(shell pkg-config --cflags gtk4)
GTK_LIBS = $(shell pkg-config --libs gtk4)

.PHONY: all clean

all: main $(COMPRESSORS) $(DECOMPRESSORS)

main: $(SOURCE) gtk_ui.h
	$(CC) $(CFLAGS) $(GTK_CFLAGS) $(SOURCE) -o main $(GTK_LIBS)

compresor_normal: compresor_normal.c aux_funcs.c aux_funcs.h compresor_normal.h
	$(CC) $(CFLAGS) compresor_normal.c aux_funcs.c -o $@

compresor_fork: compresor_fork.c
	$(CC) $(CFLAGS) $< -o $@

compresor_pthread: compresor_pthread.c
	$(CC) $(CFLAGS) $< -o $@ -pthread

descompresor_normal: descompresor_normal.c
	$(CC) $(CFLAGS) $< -o $@

descompresor_fork: descompresor_fork.c
	$(CC) $(CFLAGS) $< -o $@

descompresor_pthread: descompresor_pthread.c
	$(CC) $(CFLAGS) $< -o $@ -pthread

clean:
	rm -f main $(COMPRESSORS) $(DECOMPRESSORS) gutenberg_downloader ventana_gtk4