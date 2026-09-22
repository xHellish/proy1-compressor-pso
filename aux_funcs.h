#ifndef AUX_FUNCS_H
#define AUX_FUNCS_H

#include <stddef.h>
#include <stdint.h>

// --------------------------------------------------- //
// Definiciones

#define MAX_NODOS_HUFFMAN 511
#define HUFFMAN_MAGIC "HUF1"

// --------------------------------------------------- //
// Structs

// Entrada de archivo con su ruta y nombre
typedef struct {
	char *ruta;
	char *nombre;
} Entrada;

// Estructura de nodo para el árbol de Huffman
typedef struct {
	uint64_t frecuencia;
	int izquierda;
	int derecha;
	unsigned char simbolo;
} NodoHuffman;

// Estructura de registro comprimido con información sobre el archivo original y los datos comprimidos
typedef struct {
	char *nombre;
	uint64_t tamano_original;
	uint64_t bits;
	uint64_t frecuencias[256];
	char md5[33];
	unsigned char *datos;
	size_t datos_tamano;
} RegistroComprimido;

// Estructura de archivo comprimido que contiene múltiples registros comprimidos
typedef struct {
	RegistroComprimido *registros;
	uint32_t cantidad;
	uint64_t tamano_comprimido;
} ArchivoComprimido;

// --------------------------------------------------- //
// Declaraciones de funciones

// Funciones relacionadas con la compresión y descompresión de archivos
int leer_archivo_comprimido(const char *ruta, ArchivoComprimido *archivo);
void liberar_archivo_comprimido(ArchivoComprimido *archivo);
int descomprimir_registro(const RegistroComprimido *registro, const char *directorio);
int calcular_md5_archivo(const char *ruta, char salida[33]);
int calcular_md5_buffer(const unsigned char *datos, size_t longitud, char salida[33]);
int verificar_md5_archivo(const char *ruta, const char *esperado);

// Funciones relacionadas con la gestión de archivos y directorios
char *duplicar_texto(const char *texto);
double tiempo_monotonic(void);
void liberar_entradas(Entrada *entradas, size_t cantidad);
char *unir_ruta(const char *directorio, const char *nombre);
int listar_archivos(const char *directorio, Entrada **salida, size_t *cantidad);

#endif
