#ifndef AUX_FUNCS_H
#define AUX_FUNCS_H

#include <stddef.h>

typedef struct {
	char *ruta;
	char *nombre;
} Entrada;

char *duplicar_texto(const char *texto);
void liberar_entradas(Entrada *entradas, size_t cantidad);
char *unir_ruta(const char *directorio, const char *nombre);
int listar_archivos(const char *directorio, Entrada **salida, size_t *cantidad);

#endif
