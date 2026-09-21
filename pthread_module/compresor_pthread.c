#include "../aux_funcs.h"

#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define ARCHIVO_NORMAL "archivo_comprimido.huff"
#define ARCHIVO_PTHREAD "archivo_comprimido.huff"
#define MAGIC "HUF1"
#define ENCABEZADO_GLOBAL (4 + sizeof(uint32_t))

typedef struct {
	pthread_mutex_t mutex;
	uint32_t exitosos;
} EstadoCompartido;

typedef struct {
	const char *entrada;
	const char *salida;
	EstadoCompartido *estado;
	int correcto;
} Trabajo;

static int copiar_datos(FILE *origen, FILE *destino) {
	unsigned char buffer[8192];
	size_t leidos;
	while ((leidos = fread(buffer, 1, sizeof(buffer), origen)) > 0) {
		if (fwrite(buffer, 1, leidos, destino) != leidos) return -1;
	}
	return ferror(origen) ? -1 : 0;
}

static void *comprimir_trabajo(void *datos) {
	Trabajo *trabajo = datos;
	pid_t hijo = fork();
	int estado;

	if (hijo == 0) {
		char *argumentos[] = {"./compresor_normal", (char *)trabajo->entrada,
			(char *)trabajo->salida, NULL};
		int silencioso = open("/dev/null", O_WRONLY);
		if (silencioso >= 0) {
			dup2(silencioso, STDOUT_FILENO);
			close(silencioso);
		}
		execv(argumentos[0], argumentos);
		_exit(127);
	}
	trabajo->correcto = hijo >= 0 && waitpid(hijo, &estado, 0) >= 0 &&
		WIFEXITED(estado) && WEXITSTATUS(estado) == 0;
	if (trabajo->correcto) {
		pthread_mutex_lock(&trabajo->estado->mutex);
		++trabajo->estado->exitosos;
		pthread_mutex_unlock(&trabajo->estado->mutex);
	}
	return NULL;
}

static void limpiar_temporales(char **entradas, char **salidas, const Entrada *originales, size_t cantidad) {
	for (size_t i = 0; i < cantidad; ++i) {
		if (entradas[i] != NULL) {
			char *enlace = unir_ruta(entradas[i], originales[i].nombre);
			if (enlace != NULL) {
				remove(enlace);
				free(enlace);
			}
			rmdir(entradas[i]);
			free(entradas[i]);
		}
		if (salidas[i] != NULL) {
			char *parte = unir_ruta(salidas[i], ARCHIVO_NORMAL);
			if (parte != NULL) {
				remove(parte);
				free(parte);
			}
			rmdir(salidas[i]);
			free(salidas[i]);
		}
	}
	free(entradas);
	free(salidas);
}

int main(int argc, char **argv) {
	Entrada *archivos_entrada = NULL;
	size_t cantidad = 0;
	char **directorios_entrada = NULL;
	char **directorios_salida = NULL;
	pthread_t *hilos = NULL;
	Trabajo *trabajos = NULL;
	EstadoCompartido *estado = NULL;
	pthread_mutexattr_t atributos;
	FILE *salida = NULL;
	char *ruta_salida = NULL;
	double inicio = tiempo_monotonic();
	uint64_t original = 0;
	uint64_t comprimido = 0;
	uint32_t creados = 0;
	int resultado = 1;

	if (argc != 3) {
		fprintf(stderr, "Uso: %s <directorio_entrada> <directorio_salida>\n", argv[0]);
		return 1;
	}
	if (listar_archivos(argv[1], &archivos_entrada, &cantidad) != 0 ||
		(mkdir(argv[2], 0755) != 0 && errno != EEXIST)) goto limpiar;
	directorios_entrada = calloc(cantidad, sizeof(*directorios_entrada));
	directorios_salida = calloc(cantidad, sizeof(*directorios_salida));
	hilos = calloc(cantidad, sizeof(*hilos));
	trabajos = calloc(cantidad, sizeof(*trabajos));
	estado = mmap(NULL, sizeof(*estado), PROT_READ | PROT_WRITE,
		MAP_SHARED | MAP_ANONYMOUS, -1, 0);
	if (estado == MAP_FAILED || (cantidad != 0 && (directorios_entrada == NULL || directorios_salida == NULL ||
		hilos == NULL || trabajos == NULL))) goto limpiar;
	pthread_mutexattr_init(&atributos);
	pthread_mutexattr_setpshared(&atributos, PTHREAD_PROCESS_SHARED);
	pthread_mutex_init(&estado->mutex, &atributos);
	pthread_mutexattr_destroy(&atributos);
	for (size_t i = 0; i < cantidad; ++i) {
		char nombre[64];
		snprintf(nombre, sizeof(nombre), ".pthread_input_%zu", i);
		directorios_entrada[i] = unir_ruta(argv[2], nombre);
		snprintf(nombre, sizeof(nombre), ".pthread_part_%zu", i);
		directorios_salida[i] = unir_ruta(argv[2], nombre);
		if (directorios_entrada[i] == NULL || directorios_salida[i] == NULL ||
			mkdir(directorios_entrada[i], 0700) != 0 || mkdir(directorios_salida[i], 0700) != 0) goto limpiar;
		char *enlace = unir_ruta(directorios_entrada[i], archivos_entrada[i].nombre);
		if (enlace == NULL || link(archivos_entrada[i].ruta, enlace) != 0) {
			free(enlace);
			goto limpiar;
		}
		free(enlace);
		trabajos[i] = (Trabajo){directorios_entrada[i], directorios_salida[i], estado, 0};
		if (pthread_create(&hilos[i], NULL, comprimir_trabajo, &trabajos[i]) != 0) goto limpiar;
		++creados;
	}
	for (uint32_t i = 0; i < creados; ++i) pthread_join(hilos[i], NULL);
	if (estado->exitosos != cantidad) goto limpiar;
	ruta_salida = unir_ruta(argv[2], ARCHIVO_PTHREAD);
	salida = ruta_salida == NULL ? NULL : fopen(ruta_salida, "wb");
	if (salida == NULL || fwrite(MAGIC, 1, 4, salida) != 4 ||
		fwrite(&cantidad, sizeof(uint32_t), 1, salida) != 1) goto limpiar;
	for (size_t i = 0; i < cantidad; ++i) {
		char *ruta_parte = unir_ruta(directorios_salida[i], ARCHIVO_NORMAL);
		FILE *parte = ruta_parte == NULL ? NULL : fopen(ruta_parte, "rb");
		struct stat informacion;
		if (parte == NULL || stat(ruta_parte, &informacion) != 0 ||
			fseek(parte, (long)ENCABEZADO_GLOBAL, SEEK_SET) != 0 || copiar_datos(parte, salida) != 0) {
			if (parte != NULL) fclose(parte);
			free(ruta_parte);
			goto limpiar;
		}
		comprimido += (uint64_t)informacion.st_size - ENCABEZADO_GLOBAL;
		fclose(parte);
		free(ruta_parte);
		struct stat entrada_info;
		if (stat(archivos_entrada[i].ruta, &entrada_info) == 0) original += (uint64_t)entrada_info.st_size;
	}
	if (fclose(salida) != 0) {
		salida = NULL;
		goto limpiar;
	}
	salida = NULL;
	comprimido += ENCABEZADO_GLOBAL;
	resultado = 0;

limpiar:
	if (salida != NULL) fclose(salida);
	if (resultado != 0 && ruta_salida != NULL) remove(ruta_salida);
	if (estado != NULL && estado != MAP_FAILED) {
		pthread_mutex_destroy(&estado->mutex);
		munmap(estado, sizeof(*estado));
	}
	if (resultado == 0) {
		double ratio = original == 0 ? 0.0 : (double)comprimido / original;
		printf("RESULT|Pthread|compress|100.0|%.6f|0.00|0.00|0.00|%zu|%zu|%llu|%llu|%.6f|0\n",
			tiempo_monotonic() - inicio, cantidad, cantidad,
			(unsigned long long)original, (unsigned long long)comprimido, ratio);
	}
	free(ruta_salida);
	limpiar_temporales(directorios_entrada, directorios_salida, archivos_entrada, cantidad);
	free(hilos);
	free(trabajos);
	liberar_entradas(archivos_entrada, cantidad);
	return resultado;
}
