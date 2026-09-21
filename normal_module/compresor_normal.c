#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#include "../aux_funcs.h"
#include "compresor_normal.h"

#define ARCHIVO_SALIDA "archivo_comprimido.huff"
#define MAGIC "HUF1"

typedef struct {
	uint64_t frecuencia;
	int izquierda;
	int derecha;
	unsigned char simbolo;
} NodoHuffman;

static int construir_arbol(const uint64_t frecuencias[256], NodoHuffman nodos[511],
							int raices[256]) {
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
		int nodo_izquierdo = raices[primero];
		int nodo_derecho = raices[segundo];
		nodos[cantidad] = (NodoHuffman){
			nodos[nodo_izquierdo].frecuencia + nodos[nodo_derecho].frecuencia,
			nodo_izquierdo, nodo_derecho, 0
		};
		raices[primero] = cantidad++;
		raices[segundo] = raices[--cantidad_raices];
	}
	return cantidad_raices == 0 ? -1 : raices[0];
}

static void generar_codigos(const NodoHuffman nodos[511], int nodo, char *codigo,
							int profundidad, char *codigos[256]) {
	if (nodos[nodo].izquierda == -1) {
		codigo[profundidad == 0 ? 1 : profundidad] = '\0';
		if (profundidad == 0) {
			codigo[0] = '0';
		}
		codigos[nodos[nodo].simbolo] = duplicar_texto(codigo);
		return;
	}
	codigo[profundidad] = '0';
	generar_codigos(nodos, nodos[nodo].izquierda, codigo, profundidad + 1, codigos);
	codigo[profundidad] = '1';
	generar_codigos(nodos, nodos[nodo].derecha, codigo, profundidad + 1, codigos);
}

static int escribir_archivo(FILE *salida, const Entrada *entrada, uint64_t *tamano_original,
							uint64_t *tamano_comprimido) {
	FILE *entrada_archivo = fopen(entrada->ruta, "rb");
	uint64_t frecuencias[256] = {0};
	unsigned char *datos = NULL;
	long tamano;
	NodoHuffman nodos[511];
	char *codigos[256] = {0};
	char codigo[512];
	char md5[33];
	int raiz;
	uint64_t bits = 0;
	unsigned char byte = 0;
	int bits_en_byte = 0;
	uint32_t nombre_tamano = (uint32_t)strlen(entrada->nombre);

	if (entrada_archivo == NULL) {
		return -1;
	}
	if (fseek(entrada_archivo, 0, SEEK_END) != 0 || (tamano = ftell(entrada_archivo)) < 0 ||
		fseek(entrada_archivo, 0, SEEK_SET) != 0) {
		fclose(entrada_archivo);
		return -1;
	}
	if (tamano > 0) {
		datos = malloc((size_t)tamano);
		if (datos == NULL || fread(datos, 1, (size_t)tamano, entrada_archivo) != (size_t)tamano) {
			free(datos);
			fclose(entrada_archivo);
			return -1;
		}
	}
	fclose(entrada_archivo);
	for (long i = 0; i < tamano; ++i) {
		++frecuencias[datos[i]];
	}
	raiz = construir_arbol(frecuencias, nodos, (int[256]){0});
	if (tamano > 0) {
		generar_codigos(nodos, raiz, codigo, 0, codigos);
		for (long i = 0; i < tamano; ++i) {
			bits += strlen(codigos[datos[i]]);
		}
	}
	if (calcular_md5_buffer(datos, (size_t)tamano, md5) != 0) {
		free(datos);
		for (int i = 0; i < 256; ++i) free(codigos[i]);
		return -1;
	}
	if (fwrite(&nombre_tamano, sizeof(nombre_tamano), 1, salida) != 1 ||
		fwrite(&tamano, sizeof(tamano), 1, salida) != 1 ||
		fwrite(&bits, sizeof(bits), 1, salida) != 1 ||
		fwrite(frecuencias, sizeof(frecuencias), 1, salida) != 1 ||
		fwrite(md5, 1, sizeof(md5), salida) != sizeof(md5) ||
		fwrite(entrada->nombre, 1, nombre_tamano, salida) != nombre_tamano) {
		free(datos);
		for (int i = 0; i < 256; ++i) free(codigos[i]);
		return -1;
	}
	for (long i = 0; i < tamano; ++i) {
		for (const char *simbolo = codigos[datos[i]]; *simbolo != '\0'; ++simbolo) {
			byte = (unsigned char)((byte << 1) | (*simbolo == '1'));
			if (++bits_en_byte == 8) {
				if (fputc(byte, salida) == EOF) {
					free(datos);
					for (int j = 0; j < 256; ++j) free(codigos[j]);
					return -1;
				}
				byte = 0;
				bits_en_byte = 0;
			}
		}
	}
	if (bits_en_byte != 0 && fputc((int)(byte << (8 - bits_en_byte)), salida) == EOF) {
		free(datos);
		for (int i = 0; i < 256; ++i) free(codigos[i]);
		return -1;
	}
	*tamano_original += (uint64_t)tamano;
	*tamano_comprimido += (uint64_t)(sizeof(nombre_tamano) + sizeof(tamano) + sizeof(bits) +
								 sizeof(frecuencias) + nombre_tamano + (bits + 7) / 8);
	free(datos);
	for (int i = 0; i < 256; ++i) free(codigos[i]);
	return ferror(salida) ? -1 : 0;
}

int comprimir_huffman(const char *directorio_entrada, const char *directorio_salida) {
	Entrada *entradas = NULL;
	size_t cantidad = 0;
	char *ruta_salida;
	FILE *salida;
	uint64_t tamano_original = 0;
	uint64_t tamano_comprimido = 0;
	double inicio = tiempo_monotonic();
	int resultado = -1;

	if (listar_archivos(directorio_entrada, &entradas, &cantidad) != 0) {
		return -1;
	}
	if (mkdir(directorio_salida, 0755) != 0 && errno != EEXIST) {
		perror("No se pudo crear el directorio de salida");
		liberar_entradas(entradas, cantidad);
		return -1;
	}
	ruta_salida = unir_ruta(directorio_salida, ARCHIVO_SALIDA);
	salida = ruta_salida == NULL ? NULL : fopen(ruta_salida, "wb");
	if (salida == NULL) {
		perror("No se pudo crear el archivo comprimido");
		free(ruta_salida);
		liberar_entradas(entradas, cantidad);
		return -1;
	}
	if (fwrite(MAGIC, 1, 4, salida) != 4) goto limpiar;
	uint32_t cantidad_archivos = (uint32_t)cantidad;
	if (fwrite(&cantidad_archivos, sizeof(cantidad_archivos), 1, salida) != 1) goto limpiar;
	for (size_t i = 0; i < cantidad; ++i) {
		if (escribir_archivo(salida, &entradas[i], &tamano_original, &tamano_comprimido) != 0) {
			goto limpiar;
		}
	}
	if (fclose(salida) != 0) {
		salida = NULL;
		goto limpiar_archivo;
	}
	salida = NULL;
	resultado = 0;
	tamano_comprimido += 4 + sizeof(uint32_t);

limpiar:
	if (salida != NULL) fclose(salida);
limpiar_archivo:
	if (resultado != 0) remove(ruta_salida);
	free(ruta_salida);
	liberar_entradas(entradas, cantidad);
	if (resultado == 0) {
		double segundos = tiempo_monotonic() - inicio;
		double ratio = tamano_original == 0 ? 0.0 : (double)tamano_comprimido / tamano_original;
		printf("RESULT|Normal|compress|100.0|%.6f|0.00|0.00|0.00|%zu|%zu|%llu|%llu|%.6f|0\n",
			   segundos, cantidad, cantidad, (unsigned long long)tamano_original,
			   (unsigned long long)tamano_comprimido, ratio);
	}
	return resultado;
}

int main(int argc, char **argv) {
	if (argc != 3) {
		fprintf(stderr, "Uso: %s <directorio_entrada> <directorio_salida>\n", argv[0]);
		return 1;
	}
	return comprimir_huffman(argv[1], argv[2]) == 0 ? 0 : 1;
}
