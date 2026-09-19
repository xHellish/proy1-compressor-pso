#define _POSIX_C_SOURCE 200809L

#include "aux_funcs.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

double tiempo_monotonic(void) {
	struct timespec instante;
	clock_gettime(CLOCK_MONOTONIC, &instante);
	return (double)instante.tv_sec + (double)instante.tv_nsec / 1e9;
}

char *duplicar_texto(const char *texto) {
	size_t tamano = strlen(texto) + 1;
	char *copia = malloc(tamano);
	if (copia != NULL) {
		memcpy(copia, texto, tamano);
	}
	return copia;
}

void liberar_entradas(Entrada *entradas, size_t cantidad) {
	for (size_t i = 0; i < cantidad; ++i) {
		free(entradas[i].ruta);
		free(entradas[i].nombre);
	}
	free(entradas);
}

char *unir_ruta(const char *directorio, const char *nombre) {
	size_t tamano = strlen(directorio) + strlen(nombre) + 2;
	char *ruta = malloc(tamano);
	if (ruta != NULL) {
		snprintf(ruta, tamano, "%s/%s", directorio, nombre);
	}
	return ruta;
}

static int comparar_entradas(const void *a, const void *b) {
	const Entrada *entrada_a = a;
	const Entrada *entrada_b = b;
	return strcmp(entrada_a->nombre, entrada_b->nombre);
}

int listar_archivos(const char *directorio, Entrada **salida, size_t *cantidad) {
	DIR *dir = opendir(directorio);
	struct dirent *actual;
	Entrada *entradas = NULL;
	size_t usados = 0;

	if (dir == NULL) {
		perror("No se pudo abrir el directorio de entrada");
		return -1;
	}
	while ((actual = readdir(dir)) != NULL) {
		struct stat informacion;
		char *ruta;
		Entrada *nuevas;

		if (strcmp(actual->d_name, ".") == 0 || strcmp(actual->d_name, "..") == 0) {
			continue;
		}
		ruta = unir_ruta(directorio, actual->d_name);
		if (ruta == NULL || stat(ruta, &informacion) != 0) {
			free(ruta);
			continue;
		}
		if (!S_ISREG(informacion.st_mode)) {
			free(ruta);
			continue;
		}
		nuevas = realloc(entradas, (usados + 1) * sizeof(*entradas));
		if (nuevas == NULL) {
			free(ruta);
			closedir(dir);
			liberar_entradas(entradas, usados);
			return -1;
		}
		entradas = nuevas;
		entradas[usados].ruta = ruta;
		entradas[usados].nombre = duplicar_texto(actual->d_name);
		if (entradas[usados].nombre == NULL) {
			closedir(dir);
			liberar_entradas(entradas, usados + 1);
			return -1;
		}
		++usados;
	}
	closedir(dir);
	qsort(entradas, usados, sizeof(*entradas), comparar_entradas);
	*salida = entradas;
	*cantidad = usados;
	return 0;
}

#define MAX_NODOS_HUFFMAN 511

typedef struct {
	uint64_t frecuencia;
	int izquierda;
	int derecha;
	unsigned char simbolo;
} NodoHuffman;

static int leer_bytes(FILE *archivo, void *destino, size_t tamano) {
	return fread(destino, 1, tamano, archivo) == tamano ? 0 : -1;
}

static void liberar_registro(RegistroComprimido *registro) {
	free(registro->nombre);
	free(registro->datos);
	memset(registro, 0, sizeof(*registro));
}

static int construir_arbol_descompresion(const uint64_t frecuencias[256],
							NodoHuffman nodos[MAX_NODOS_HUFFMAN]) {
	int raices[256];
	int cantidad = 0;
	int cantidad_raices = 0;

	for (int simbolo = 0; simbolo < 256; ++simbolo) {
		if (frecuencias[simbolo] != 0) {
			nodos[cantidad] = (NodoHuffman){frecuencias[simbolo], -1, -1, (unsigned char)simbolo};
			raices[cantidad_raices++] = cantidad++;
		}
	}
	while (cantidad_raices > 1) {
		int primero = 0;
		int segundo = 1;
		if (nodos[raices[segundo]].frecuencia < nodos[raices[primero]].frecuencia) {
			int temporal = primero;
			primero = segundo;
			segundo = temporal;
		}
		for (int i = 2; i < cantidad_raices; ++i) {
			if (nodos[raices[i]].frecuencia < nodos[raices[primero]].frecuencia) {
				segundo = primero;
				primero = i;
			} else if (nodos[raices[i]].frecuencia < nodos[raices[segundo]].frecuencia) {
				segundo = i;
			}
		}
		int izquierda = raices[primero];
		int derecha = raices[segundo];
		nodos[cantidad] = (NodoHuffman){
			nodos[izquierda].frecuencia + nodos[derecha].frecuencia,
			izquierda, derecha, 0
		};
		raices[primero] = cantidad++;
		raices[segundo] = raices[--cantidad_raices];
	}
	return cantidad_raices == 0 ? -1 : raices[0];
}

int leer_archivo_comprimido(const char *ruta, ArchivoComprimido *archivo) {
	FILE *entrada = fopen(ruta, "rb");
	char magic[4];
	uint32_t cantidad;

	memset(archivo, 0, sizeof(*archivo));
	if (entrada == NULL || leer_bytes(entrada, magic, sizeof(magic)) != 0 ||
		memcmp(magic, HUFFMAN_MAGIC, sizeof(magic)) != 0 ||
		leer_bytes(entrada, &cantidad, sizeof(cantidad)) != 0) {
		if (entrada != NULL) fclose(entrada);
		return -1;
	}
	archivo->registros = calloc(cantidad, sizeof(*archivo->registros));
	if (cantidad != 0 && archivo->registros == NULL) {
		fclose(entrada);
		return -1;
	}
	archivo->cantidad = cantidad;
	for (uint32_t i = 0; i < cantidad; ++i) {
		RegistroComprimido *registro = &archivo->registros[i];
		uint32_t nombre_tamano;
		if (leer_bytes(entrada, &nombre_tamano, sizeof(nombre_tamano)) != 0 || nombre_tamano == 0 ||
			leer_bytes(entrada, &registro->tamano_original, sizeof(registro->tamano_original)) != 0 ||
			leer_bytes(entrada, &registro->bits, sizeof(registro->bits)) != 0 ||
			leer_bytes(entrada, registro->frecuencias, sizeof(registro->frecuencias)) != 0 ||
			registro->bits > UINT64_MAX - 7) {
			fclose(entrada);
			liberar_archivo_comprimido(archivo);
			return -1;
		}
		registro->nombre = malloc((size_t)nombre_tamano + 1);
		if (registro->nombre == NULL || leer_bytes(entrada, registro->nombre, nombre_tamano) != 0) {
			fclose(entrada);
			liberar_archivo_comprimido(archivo);
			return -1;
		}
		registro->nombre[nombre_tamano] = '\0';
		registro->datos_tamano = (size_t)((registro->bits + 7) / 8);
		registro->datos = malloc(registro->datos_tamano == 0 ? 1 : registro->datos_tamano);
		if (registro->datos == NULL || leer_bytes(entrada, registro->datos, registro->datos_tamano) != 0) {
			fclose(entrada);
			liberar_archivo_comprimido(archivo);
			return -1;
		}
		archivo->tamano_comprimido += sizeof(nombre_tamano) + sizeof(registro->tamano_original) +
			sizeof(registro->bits) + sizeof(registro->frecuencias) + nombre_tamano + registro->datos_tamano;
	}
	fclose(entrada);
	archivo->tamano_comprimido += sizeof(magic) + sizeof(cantidad);
	return 0;
}

void liberar_archivo_comprimido(ArchivoComprimido *archivo) {
	for (uint32_t i = 0; i < archivo->cantidad; ++i) liberar_registro(&archivo->registros[i]);
	free(archivo->registros);
	memset(archivo, 0, sizeof(*archivo));
}

int descomprimir_registro(const RegistroComprimido *registro, const char *directorio) {
	char *ruta = unir_ruta(directorio, registro->nombre);
	FILE *salida;
	NodoHuffman nodos[MAX_NODOS_HUFFMAN];
	int raiz;
	int nodo;
	uint64_t producidos = 0;

	if (ruta == NULL) return -1;
	salida = fopen(ruta, "wb");
	free(ruta);
	if (salida == NULL) return -1;
	if (registro->tamano_original == 0) return fclose(salida) == 0 ? 0 : -1;
	raiz = construir_arbol_descompresion(registro->frecuencias, nodos);
	if (raiz < 0) {
		fclose(salida);
		return -1;
	}
	if (nodos[raiz].izquierda == -1) {
		for (uint64_t i = 0; i < registro->tamano_original; ++i) {
			if (fputc(nodos[raiz].simbolo, salida) == EOF) {
				fclose(salida);
				return -1;
			}
		}
		return fclose(salida) == 0 ? 0 : -1;
	}
	nodo = raiz;
	for (uint64_t bit = 0; bit < registro->bits && producidos < registro->tamano_original; ++bit) {
		unsigned char byte = registro->datos[bit / 8];
		nodo = ((byte >> (7 - (bit % 8))) & 1) == 0 ? nodos[nodo].izquierda : nodos[nodo].derecha;
		if (nodo < 0 || nodo >= MAX_NODOS_HUFFMAN) {
			fclose(salida);
			return -1;
		}
		if (nodos[nodo].izquierda == -1) {
			if (fputc(nodos[nodo].simbolo, salida) == EOF) {
				fclose(salida);
				return -1;
			}
			++producidos;
			nodo = raiz;
		}
	}
	if (producidos != registro->tamano_original || fclose(salida) != 0) return -1;
	return 0;
}
