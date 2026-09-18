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

compresor_normal: normal_module/compresor_normal.c normal_module/compresor_normal.h aux_funcs.c aux_funcs.h
	$(CC) $(CFLAGS) normal_module/compresor_normal.c aux_funcs.c -o $@

compresor_fork: fork_module/compresor_fork.c fork_module/compresor_fork.h aux_funcs.c aux_funcs.h compresor_normal
	$(CC) $(CFLAGS) fork_module/compresor_fork.c aux_funcs.c -o $@

compresor_pthread: pthread_module/compresor_pthread.c aux_funcs.c aux_funcs.h compresor_normal
	$(CC) $(CFLAGS) $< aux_funcs.c -o $@ -pthread

descompresor_normal: normal_module/descompresor_normal.c normal_module/descompresor_normal.h aux_funcs.c aux_funcs.h
	$(CC) $(CFLAGS) normal_module/descompresor_normal.c aux_funcs.c -o $@

descompresor_fork: fork_module/descompresor_fork.c fork_module/descompresor_fork.h aux_funcs.c aux_funcs.h
	$(CC) $(CFLAGS) fork_module/descompresor_fork.c aux_funcs.c -o $@

descompresor_pthread: pthread_module/descompresor_pthread.c aux_funcs.c aux_funcs.h
	$(CC) $(CFLAGS) $< aux_funcs.c -o $@ -pthread

clean:
	rm -f main $(COMPRESSORS) $(DECOMPRESSORS) gutenberg_downloader ventana_gtk4