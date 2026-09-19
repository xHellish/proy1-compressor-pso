#ifndef AUX_FUNCS_H
#define AUX_FUNCS_H

#include <stddef.h>
#include <stdint.h>

typedef struct {
	char *ruta;
	char *nombre;
} Entrada;

char *duplicar_texto(const char *texto);
double tiempo_monotonic(void);
void liberar_entradas(Entrada *entradas, size_t cantidad);
char *unir_ruta(const char *directorio, const char *nombre);
int listar_archivos(const char *directorio, Entrada **salida, size_t *cantidad);

#define HUFFMAN_MAGIC "HUF1"

typedef struct {
	char *nombre;
	uint64_t tamano_original;
	uint64_t bits;
	uint64_t frecuencias[256];
	unsigned char *datos;
	size_t datos_tamano;
} RegistroComprimido;

typedef struct {
	RegistroComprimido *registros;
	uint32_t cantidad;
	uint64_t tamano_comprimido;
} ArchivoComprimido;

int leer_archivo_comprimido(const char *ruta, ArchivoComprimido *archivo);
void liberar_archivo_comprimido(ArchivoComprimido *archivo);
int descomprimir_registro(const RegistroComprimido *registro, const char *directorio);

#endif
