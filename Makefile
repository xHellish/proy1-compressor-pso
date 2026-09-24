CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -Wpedantic
OPENSSL_LIBS = -lcrypto
SOURCE = main.c gtk_ui.c
COMPRESSORS = compresor_normal compresor_fork compresor_pthread
DECOMPRESSORS = descompresor_normal descompresor_fork descompresor_pthread
GTK_CFLAGS = $(shell pkg-config --cflags gtk4)
GTK_LIBS = $(shell pkg-config --libs gtk4)
# Directorio con los binarios ayudantes visto desde el binario "main".
# Build normal (sin definir nada): HELPER_DIR="." -> "./compresor_normal" (como antes).
# Build Flatpak: make HELPER_DIR=/app/bin -> "/app/bin/compresor_normal".
HELPER_DIR ?= .
HELPER_DEFINE = -DHELPER_DIR='"$(HELPER_DIR)"'
# Prefijo de instalación para Flatpak.
# Se respeta DESTDIR estándar: `make install DESTDIR=/app` instala en /app/bin
# dentro del sandbox de flatpak-builder (BINDIR = $(DESTDIR)/bin).
# Solo lo usa el manifiesto Flatpak; `make` normal no instala nada.
BINDIR = $(DESTDIR)/bin

.PHONY: all clean install

all: main $(COMPRESSORS) $(DECOMPRESSORS)

main: $(SOURCE) gtk_ui.h
	$(CC) $(CFLAGS) $(HELPER_DEFINE) $(GTK_CFLAGS) $(SOURCE) -o main $(GTK_LIBS)

compresor_normal: normal_module/compresor_normal.c normal_module/compresor_normal.h aux_funcs.c aux_funcs.h
	$(CC) $(CFLAGS) $(HELPER_DEFINE) normal_module/compresor_normal.c aux_funcs.c -o $@ $(OPENSSL_LIBS)

compresor_fork: fork_module/compresor_fork.c fork_module/compresor_fork.h aux_funcs.c aux_funcs.h compresor_normal
	$(CC) $(CFLAGS) $(HELPER_DEFINE) fork_module/compresor_fork.c aux_funcs.c -o $@ $(OPENSSL_LIBS)

compresor_pthread: pthread_module/compresor_pthread.c aux_funcs.c aux_funcs.h compresor_normal
	$(CC) $(CFLAGS) $(HELPER_DEFINE) $< aux_funcs.c -o $@ -pthread $(OPENSSL_LIBS)

descompresor_normal: normal_module/descompresor_normal.c normal_module/descompresor_normal.h aux_funcs.c aux_funcs.h
	$(CC) $(CFLAGS) $(HELPER_DEFINE) normal_module/descompresor_normal.c aux_funcs.c -o $@ $(OPENSSL_LIBS)

descompresor_fork: fork_module/descompresor_fork.c fork_module/descompresor_fork.h aux_funcs.c aux_funcs.h
	$(CC) $(CFLAGS) $(HELPER_DEFINE) fork_module/descompresor_fork.c aux_funcs.c -o $@ $(OPENSSL_LIBS)

descompresor_pthread: pthread_module/descompresor_pthread.c aux_funcs.c aux_funcs.h
	$(CC) $(CFLAGS) $(HELPER_DEFINE) $< aux_funcs.c -o $@ -pthread $(OPENSSL_LIBS)

# Instalación opcional (solo la usa el manifiesto Flatpak con DESTDIR=/app).
# Respeta DESTDIR estándar: `make install DESTDIR=/tmp/stage` instala en /tmp/stage/bin,
# y `make install DESTDIR=/app` instala en /app/bin (equivale a /app/bin en el Flatpak).
# `make` normal no instala nada: el flujo make / ./main queda intacto.
install: all
	mkdir -p $(BINDIR)
	install -m 755 main $(COMPRESSORS) $(DECOMPRESSORS) $(BINDIR)/

clean:
	rm -f main $(COMPRESSORS) $(DECOMPRESSORS) gutenberg_downloader ventana_gtk4