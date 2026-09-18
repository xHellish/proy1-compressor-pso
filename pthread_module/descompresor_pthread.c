#include "../aux_funcs.h"

#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>

#define ARCHIVO_SALIDA "pthread"

typedef struct {
	pthread_mutex_t mutex;
	uint32_t verificadas;
} EstadoCompartido;

typedef struct {
	const RegistroComprimido *registro;
	const char *directorio;
	EstadoCompartido *estado;
} Trabajo;

static void *descomprimir_trabajo(void *datos) {
	Trabajo *trabajo = datos;
	int correcto = descomprimir_registro(trabajo->registro, trabajo->directorio) == 0;
	if (correcto) {
		pthread_mutex_lock(&trabajo->estado->mutex);
		++trabajo->estado->verificadas;
		pthread_mutex_unlock(&trabajo->estado->mutex);
	}
	return NULL;
}

int main(int argc, char **argv) {
	ArchivoComprimido archivo;
	char *directorio_resultado;
	pthread_t *hilos = NULL;
	Trabajo *trabajos = NULL;
	EstadoCompartido *estado = NULL;
	pthread_mutexattr_t atributos;
	clock_t inicio = clock();
	uint64_t original = 0;
	uint32_t creados = 0;
	uint32_t verificadas = 0;
	int resultado = 1;

	if (argc != 3) {
		fprintf(stderr, "Uso: %s <archivo_comprimido> <directorio_destino>\n", argv[0]);
		return 1;
	}
	if (mkdir(argv[2], 0755) != 0 && errno != EEXIST) return 1;
	directorio_resultado = unir_ruta(argv[2], ARCHIVO_SALIDA);
	if (directorio_resultado == NULL || (mkdir(directorio_resultado, 0755) != 0 && errno != EEXIST) ||
		leer_archivo_comprimido(argv[1], &archivo) != 0) {
		free(directorio_resultado);
		return 1;
	}
	for (uint32_t i = 0; i < archivo.cantidad; ++i) original += archivo.registros[i].tamano_original;
	estado = mmap(NULL, sizeof(*estado), PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (estado == MAP_FAILED) goto limpiar;
	pthread_mutexattr_init(&atributos);
	pthread_mutexattr_setpshared(&atributos, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&estado->mutex, &atributos);
	pthread_mutexattr_destroy(&atributos);
	hilos = calloc(archivo.cantidad, sizeof(*hilos));
	trabajos = calloc(archivo.cantidad, sizeof(*trabajos));
	if ((archivo.cantidad != 0 && hilos == NULL) || (archivo.cantidad != 0 && trabajos == NULL)) goto limpiar;
	for (uint32_t i = 0; i < archivo.cantidad; ++i) {
		trabajos[i] = (Trabajo){&archivo.registros[i], directorio_resultado, estado};
		if (pthread_create(&hilos[i], NULL, descomprimir_trabajo, &trabajos[i]) != 0) break;
		++creados;
	}
	for (uint32_t i = 0; i < creados; ++i) pthread_join(hilos[i], NULL);
	verificadas = estado->verificadas;
	resultado = verificadas == archivo.cantidad ? 0 : 1;

limpiar:
	if (estado != NULL && estado != MAP_FAILED) {
		pthread_mutex_destroy(&estado->mutex);
		munmap(estado, sizeof(*estado));
	}
	if (resultado == 0) {
		double ratio = original == 0 ? 0.0 : (double)archivo.tamano_comprimido / original;
		printf("RESULT|Pthread|decompress|%.1f|0.00|%.6f|0.00|0.00|%u|%u|%llu|%llu|%.6f|0\n",
			100.0 * (double)verificadas / (archivo.cantidad == 0 ? 1 : archivo.cantidad),
			(double)(clock() - inicio) / CLOCKS_PER_SEC, archivo.cantidad, verificadas,
			(unsigned long long)original, (unsigned long long)archivo.tamano_comprimido, ratio);
	}
	free(hilos);
	free(trabajos);
	liberar_archivo_comprimido(&archivo);
	free(directorio_resultado);
	return resultado;
}
