#include "descompresor_fork.h"
#include "../aux_funcs.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int descomprimir_fork(const char *archivo_comprimido, const char *directorio_destino) {
	ArchivoComprimido archivo;
	char *directorio_resultado = unir_ruta(directorio_destino, "fork");
	clock_t inicio = clock();
	int canal[2];
	uint32_t verificadas = 0;
	uint64_t original = 0;

	if (mkdir(directorio_destino, 0755) != 0 && errno != EEXIST) return -1;
	if (directorio_resultado == NULL ||
		(mkdir(directorio_resultado, 0755) != 0 && errno != EEXIST)) {
		free(directorio_resultado);
		return -1;
	}
	if (leer_archivo_comprimido(archivo_comprimido, &archivo) != 0 || pipe(canal) != 0) return -1;
	for (uint32_t i = 0; i < archivo.cantidad; ++i) original += archivo.registros[i].tamano_original;
	for (uint32_t i = 0; i < archivo.cantidad; ++i) {
		pid_t pid = fork();
		if (pid == 0) {
			int estado = descomprimir_registro(&archivo.registros[i], directorio_resultado) == 0 ? 1 : 0;
			close(canal[0]);
			(void)write(canal[1], &estado, sizeof(estado));
			close(canal[1]);
			_exit(estado == 1 ? 0 : 1);
		}
		if (pid < 0) {
			int estado = 0;
			(void)write(canal[1], &estado, sizeof(estado));
		}
	}
	close(canal[1]);
	for (uint32_t i = 0; i < archivo.cantidad; ++i) {
		int estado;
		if (read(canal[0], &estado, sizeof(estado)) == (ssize_t)sizeof(estado)) verificadas += estado == 1;
	}
	close(canal[0]);
	while (wait(NULL) > 0) {}
	int resultado = verificadas == archivo.cantidad ? 0 : -1;
	double segundos = (double)(clock() - inicio) / CLOCKS_PER_SEC;
	double ratio = original == 0 ? 0.0 : (double)archivo.tamano_comprimido / original;
	printf("RESULT|Fork|decompress|%.1f|0.00|%.6f|0.00|0.00|%u|%u|%llu|%llu|%.6f|%d\n",
		100.0 * verificadas / (archivo.cantidad == 0 ? 1 : archivo.cantidad), segundos,
		archivo.cantidad, verificadas, (unsigned long long)original,
		(unsigned long long)archivo.tamano_comprimido, ratio, resultado == 0 ? 0 : 1);
	liberar_archivo_comprimido(&archivo);
	free(directorio_resultado);
	return resultado;
}

int main(int argc, char **argv) {
	if (argc != 3) {
		fprintf(stderr, "Uso: %s <archivo_comprimido> <directorio_destino>\n", argv[0]);
		return 1;
	}
	return descomprimir_fork(argv[1], argv[2]) == 0 ? 0 : 1;
}
