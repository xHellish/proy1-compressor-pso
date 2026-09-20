#include "descompresor_normal.h"
#include "../aux_funcs.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <time.h>

int descomprimir_normal(const char *archivo_comprimido, const char *directorio_destino) {
	ArchivoComprimido archivo;
	char *directorio_resultado = unir_ruta(directorio_destino, "normal");
	double inicio = tiempo_monotonic();
	uint32_t verificadas = 0;
	uint64_t original = 0;

	if (mkdir(directorio_destino, 0755) != 0 && errno != EEXIST) return -1;
	if (directorio_resultado == NULL ||
		(mkdir(directorio_resultado, 0755) != 0 && errno != EEXIST)) {
		free(directorio_resultado);
		return -1;
	}
	if (leer_archivo_comprimido(archivo_comprimido, &archivo) != 0) return -1;
	for (uint32_t i = 0; i < archivo.cantidad; ++i) {
		char *ruta_archivo = unir_ruta(directorio_resultado, archivo.registros[i].nombre);
		int correcto = ruta_archivo != NULL &&
			descomprimir_registro(&archivo.registros[i], directorio_resultado) == 0 &&
			verificar_md5_archivo(ruta_archivo, archivo.registros[i].md5) == 0;
		original += archivo.registros[i].tamano_original;
		if (correcto) ++verificadas;
		free(ruta_archivo);
	}
	int resultado = verificadas == archivo.cantidad ? 0 : -1;
	double segundos = tiempo_monotonic() - inicio;
	double ratio = original == 0 ? 0.0 : (double)archivo.tamano_comprimido / original;
	printf("RESULT|Normal|decompress|%.1f|0.00|%.6f|0.00|0.00|%u|%u|%llu|%llu|%.6f|%d\n",
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
	return descomprimir_normal(argv[1], argv[2]) == 0 ? 0 : 1;
}
