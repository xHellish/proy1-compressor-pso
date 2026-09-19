#include "compresor_fork.h"
#include "../aux_funcs.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ARCHIVO_SALIDA "archivo_comprimido_fork.huff"
#define ARCHIVO_SALIDA_NORMAL "archivo_comprimido_normal.huff"
#define MAGIC "HUF1"
#define TAMANO_ENCABEZADO (sizeof(uint32_t) + 4)

static int copiar_datos(FILE *origen, FILE *destino) {
	unsigned char buffer[8192];
	size_t leidos;
	while ((leidos = fread(buffer, 1, sizeof(buffer), origen)) > 0) {
		if (fwrite(buffer, 1, leidos, destino) != leidos) return -1;
	}
	return ferror(origen) ? -1 : 0;
}

static void limpiar_temporales(char **directorios, char **archivos, const Entrada *entradas, size_t cantidad) {
	for (size_t i = 0; i < cantidad; ++i) {
		if (directorios[i] != NULL) {
			char *entrada = unir_ruta(directorios[i], entradas[i].nombre);
			if (entrada != NULL) {
				remove(entrada);
				free(entrada);
			}
			rmdir(directorios[i]);
		}
		if (archivos[i] != NULL) {
			char *parte = unir_ruta(archivos[i], ARCHIVO_SALIDA_NORMAL);
			if (parte != NULL) {
				remove(parte);
				free(parte);
			}
			rmdir(archivos[i]);
			free(archivos[i]);
		}
		free(directorios[i]);
	}
	free(directorios);
	free(archivos);
}

int comprimir_fork(const char *directorio_entrada, const char *directorio_salida) {
	Entrada *entradas = NULL;
	size_t cantidad = 0;
	char **temporales = NULL;
	char **archivos = NULL;
	int *estados = NULL;
	int canal[2];
	FILE *salida = NULL;
	double inicio = tiempo_monotonic();
	int resultado = -1;
	uint64_t tamano_original = 0;
	uint64_t tamano_comprimido = 0;

	if (listar_archivos(directorio_entrada, &entradas, &cantidad) != 0 ||
		(mkdir(directorio_salida, 0755) != 0 && errno != EEXIST) || pipe(canal) != 0) {
		liberar_entradas(entradas, cantidad);
		return -1;
	}
	temporales = calloc(cantidad, sizeof(*temporales));
	archivos = calloc(cantidad, sizeof(*archivos));
	estados = calloc(cantidad, sizeof(*estados));
	if ((cantidad != 0 && temporales == NULL) || (cantidad != 0 && archivos == NULL) ||
		(cantidad != 0 && estados == NULL)) goto limpiar;
	for (size_t i = 0; i < cantidad; ++i) {
		char nombre_temporal[64];
		snprintf(nombre_temporal, sizeof(nombre_temporal), ".fork_input_%zu", i);
		temporales[i] = unir_ruta(directorio_salida, nombre_temporal);
		snprintf(nombre_temporal, sizeof(nombre_temporal), ".fork_part_%zu.huff", i);
		archivos[i] = unir_ruta(directorio_salida, nombre_temporal);
		if (temporales[i] == NULL || archivos[i] == NULL || mkdir(temporales[i], 0700) != 0 ||
			mkdir(archivos[i], 0700) != 0) goto limpiar;
		char *enlace = unir_ruta(temporales[i], entradas[i].nombre);
		if (enlace == NULL || link(entradas[i].ruta, enlace) != 0) {
			free(enlace);
			goto limpiar;
		}
		free(enlace);
		pid_t hijo = fork();
		if (hijo == 0) {
			char *argumentos[] = {"./compresor_normal", temporales[i], archivos[i], NULL};
			int silencioso = open("/dev/null", O_WRONLY);
			close(canal[0]);
			if (silencioso >= 0) {
				dup2(silencioso, STDOUT_FILENO);
				close(silencioso);
			}
			execv(argumentos[0], argumentos);
			_exit(127);
		}
		if (hijo < 0) {
			int estado = 0;
			(void)write(canal[1], &estado, sizeof(estado));
		} else {
			int estado = 1;
			(void)write(canal[1], &estado, sizeof(estado));
		}
	}
	close(canal[1]);
	for (size_t i = 0; i < cantidad; ++i) {
		int estado;
		if (read(canal[0], &estado, sizeof(estado)) == (ssize_t)sizeof(estado)) estados[i] = estado;
	}
	close(canal[0]);
	while (wait(NULL) > 0) {}
	for (size_t i = 0; i < cantidad; ++i) {
		char *generado = unir_ruta(archivos[i], ARCHIVO_SALIDA_NORMAL);
		if (generado == NULL) goto limpiar;
		free(generado);
	}
	char *ruta_salida = unir_ruta(directorio_salida, ARCHIVO_SALIDA);
	salida = ruta_salida == NULL ? NULL : fopen(ruta_salida, "wb");
	free(ruta_salida);
	if (salida == NULL || fwrite(MAGIC, 1, 4, salida) != 4 ||
		fwrite(&cantidad, sizeof(uint32_t), 1, salida) != 1) goto limpiar;
	for (size_t i = 0; i < cantidad; ++i) {
		char *ruta_parte = unir_ruta(archivos[i], ARCHIVO_SALIDA_NORMAL);
		FILE *parte = ruta_parte == NULL ? NULL : fopen(ruta_parte, "rb");
		if (parte == NULL || fseek(parte, (long)TAMANO_ENCABEZADO, SEEK_SET) != 0 || copiar_datos(parte, salida) != 0) {
			if (parte != NULL) fclose(parte);
			free(ruta_parte);
			goto limpiar;
		}
		fclose(parte);
		free(ruta_parte);
	}
	if (fclose(salida) != 0) {
		salida = NULL;
		goto limpiar;
	}
	salida = NULL;
	resultado = 0;
	for (size_t i = 0; i < cantidad; ++i) {
		struct stat informacion;
		if (stat(entradas[i].ruta, &informacion) == 0) tamano_original += (uint64_t)informacion.st_size;
	}
	struct stat informacion_salida;
	char *ruta_final = unir_ruta(directorio_salida, ARCHIVO_SALIDA);
	if (ruta_final != NULL && stat(ruta_final, &informacion_salida) == 0) {
		tamano_comprimido = (uint64_t)informacion_salida.st_size;
	}
	free(ruta_final);

limpiar:
	if (salida != NULL) fclose(salida);
	limpiar_temporales(temporales, archivos, entradas, cantidad);
	free(estados);
	liberar_entradas(entradas, cantidad);
	if (resultado == 0) {
		double ratio = tamano_original == 0 ? 0.0 : (double)tamano_comprimido / tamano_original;
		printf("RESULT|Fork|compress|100.0|%.6f|0.00|0.00|0.00|%zu|%zu|%llu|%llu|%.6f|0\n",
			tiempo_monotonic() - inicio, cantidad, cantidad,
			(unsigned long long)tamano_original, (unsigned long long)tamano_comprimido, ratio);
	}
	return resultado;
}

int main(int argc, char **argv) {
	if (argc != 3) {
		fprintf(stderr, "Uso: %s <directorio_entrada> <directorio_salida>\n", argv[0]);
		return 1;
	}
	return comprimir_fork(argv[1], argv[2]) == 0 ? 0 : 1;
}
