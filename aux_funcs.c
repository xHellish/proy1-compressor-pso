#include "aux_funcs.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
