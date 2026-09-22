
#define _POSIX_C_SOURCE 200809L  // Para asegurar la disponibilidad de funciones POSIX como clock_gettime y otras

// --------------------------------------------------- // 
// Bibliotecas
#include <dirent.h>  // Para trabajar con directorios y sus entradas
#include <openssl/md5.h>  // Para calcular el hash MD5
#include <stdio.h>
#include <stdlib.h> 
#include <string.h>
#include <sys/stat.h>  // Para obtener información sobre archivos y directorios
#include <time.h>

#include "aux_funcs.h"

// Funciones relacionadas a la medición de tiempo y cálculo de hash MD5.

// Calcula el tiempo monotónico en segundos.
double tiempo_monotonic(void) {
	struct timespec instante;
	clock_gettime(CLOCK_MONOTONIC, &instante);
	return (double)instante.tv_sec + (double)instante.tv_nsec / 1e9;
}

// Calcula el hash MD5 de un buffer de datos.
int calcular_md5_buffer(const unsigned char *datos, size_t longitud, char salida[33]) {
	unsigned char digest[MD5_DIGEST_LENGTH];
	MD5_CTX contexto;

	// Valida el puntero y la longitud del buffer.
	if (datos == NULL && longitud != 0) return -1;

	MD5_Init(&contexto);

	// Actualiza el contexto solo si hay datos para procesar.
	if (longitud > 0) {
		MD5_Update(&contexto, datos, longitud);
	}

	MD5_Final(digest, &contexto);

	// Convierte el digest a hexadecimal.
	for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
		snprintf(salida + (size_t)i * 2, 3, "%02x", digest[i]);
	}

	salida[32] = '\0';
	return 0;
}

// Calcula el hash MD5 de un archivo.
int calcular_md5_archivo(const char *ruta, char salida[33]) {
	FILE *archivo = fopen(ruta, "rb");
	unsigned char buffer[8192];
	size_t leidos;
	MD5_CTX contexto;
	unsigned char digest[MD5_DIGEST_LENGTH];

	// Verifica que el archivo pueda abrirse.
	if (archivo == NULL) return -1;

	MD5_Init(&contexto);

	// Lee el archivo en bloques y actualiza el hash progresivamente.
	while ((leidos = fread(buffer, 1, sizeof(buffer), archivo)) > 0) {
		MD5_Update(&contexto, buffer, leidos);
	}

	// Detecta errores de lectura del archivo.
	if (ferror(archivo) != 0) {
		fclose(archivo);
		return -1;
	}

	fclose(archivo);
	MD5_Final(digest, &contexto);

	// Convierte cada byte del digest a hexadecimal.
	for (int i = 0; i < MD5_DIGEST_LENGTH; ++i) {
		snprintf(salida + (size_t)i * 2, 3, "%02x", digest[i]);
	}

	salida[32] = '\0';
	return 0;
}

// Verifica si el hash MD5 de un archivo coincide con el valor esperado.
int verificar_md5_archivo(const char *ruta, const char *esperado) {
	char actual[33];

	// Valida los parámetros de entrada.
	if (ruta == NULL || esperado == NULL) return -1;

	// Calcula el hash actual del archivo.
	if (calcular_md5_archivo(ruta, actual) != 0) return -1;

	return strcmp(actual, esperado) == 0 ? 0 : -1;
}

// Funciones relacionadas con la gestión de archivos y directorios.

// Duplica una cadena de texto en memoria nueva.
char *duplicar_texto(const char *texto) {
	size_t tamano = strlen(texto) + 1;
	char *copia = malloc(tamano);

	// Copia el contenido original en la nueva memoria.
	if (copia != NULL) {
		memcpy(copia, texto, tamano);
	}

	return copia;
}

// Libera la memoria ocupada por un arreglo de entradas.
void liberar_entradas(Entrada *entradas, size_t cantidad) {
	// Libera cada ruta y nombre almacenado.
	for (size_t i = 0; i < cantidad; ++i) {
		free(entradas[i].ruta);
		free(entradas[i].nombre);
	}

	free(entradas);
}

// Construye una ruta completa combinando un directorio y un nombre de archivo.
char *unir_ruta(const char *directorio, const char *nombre) {
	size_t tamano = strlen(directorio) + strlen(nombre) + 2;
	char *ruta = malloc(tamano);

	// Genera la ruta con formato "directorio/nombre".
	if (ruta != NULL) {
		snprintf(ruta, tamano, "%s/%s", directorio, nombre);
	}

	return ruta;
}

// Compara dos entradas por nombre para ordenarlas alfabéticamente.
static int comparar_entradas(const void *a, const void *b) {
	const Entrada *entrada_a = a;
	const Entrada *entrada_b = b;
	return strcmp(entrada_a->nombre, entrada_b->nombre);
}

// Lista los archivos regulares de un directorio y los devuelve ordenados.
int listar_archivos(const char *directorio, Entrada **salida, size_t *cantidad) {
	DIR *dir = opendir(directorio);
	struct dirent *actual;
	Entrada *entradas = NULL;
	size_t usados = 0;

	// Verifica que el directorio exista y pueda abrirse.
	if (dir == NULL) {
		perror("No se pudo abrir el directorio de entrada");
		return -1;
	}

	// Recorre cada entrada del directorio.
	while ((actual = readdir(dir)) != NULL) {
		struct stat informacion;
		char *ruta;
		Entrada *nuevas;

		// Ignora los directorios actuales y padres.
		if (strcmp(actual->d_name, ".") == 0 || strcmp(actual->d_name, "..") == 0) {
			continue;
		}

		ruta = unir_ruta(directorio, actual->d_name);

		// Omite entradas inválidas o que no puedan consultarse.
		if (ruta == NULL || stat(ruta, &informacion) != 0) {
			free(ruta);
			continue;
		}

		// Solo se aceptan archivos regulares.
		if (!S_ISREG(informacion.st_mode)) {
			free(ruta);
			continue;
		}

		nuevas = realloc(entradas, (usados + 1) * sizeof(*entradas));

		// Si no hay memoria suficiente, libera recursos y aborta.
		if (nuevas == NULL) {
			free(ruta);
			closedir(dir);
			liberar_entradas(entradas, usados);
			return -1;
		}

		entradas = nuevas;
		entradas[usados].ruta = ruta;
		entradas[usados].nombre = duplicar_texto(actual->d_name);

		// Si falla la copia del nombre, libera la entrada parcial.
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

// Funciones relacionadas con la compresión y descompresión de archivos.

// Lee exactamente `tamano` bytes desde un archivo.
static int leer_bytes(FILE *archivo, void *destino, size_t tamano) {
	return fread(destino, 1, tamano, archivo) == tamano ? 0 : -1;
}

// Libera la memoria asociada a un registro comprimido.
static void liberar_registro(RegistroComprimido *registro) {
	free(registro->nombre);
	free(registro->datos);
	memset(registro, 0, sizeof(*registro));
}

// Reconstruye el árbol de Huffman a partir de las frecuencias de cada símbolo.
static int construir_arbol_descompresion(const uint64_t frecuencias[256], NodoHuffman nodos[MAX_NODOS_HUFFMAN]) {
	
	int raices[256];
	int cantidad = 0;
	int cantidad_raices = 0;

	// Inicializa los nodos del árbol con las frecuencias observadas.
	for (int simbolo = 0; simbolo < 256; ++simbolo) {

		if (frecuencias[simbolo] != 0) {  // Si la frecuencia del símbolo es diferente de cero, se crea un nodo para ese símbolo y se agrega a la lista de raíces
			nodos[cantidad] = (NodoHuffman){frecuencias[simbolo], -1, -1, (unsigned char)simbolo};
			raices[cantidad_raices++] = cantidad++;
		}
	}

	// Construye el árbol combinando los nodos con menor frecuencia.
	while (cantidad_raices > 1) {

		int primero = 0;
		int segundo = 1;

		if (nodos[raices[segundo]].frecuencia < nodos[raices[primero]].frecuencia) {  // Intercambia los índices de los dos nodos con las frecuencias más bajas si es necesario para asegurarse de que primero tenga la frecuencia más baja
			int temporal = primero;
			primero = segundo;
			segundo = temporal;
		}

		// Encuentra los dos nodos con las frecuencias más bajas en la lista de raíces
		for (int i = 2; i < cantidad_raices; ++i) {

			// Compara la frecuencia del nodo actual con las frecuencias de los dos nodos más bajos encontrados hasta ahora
			if (nodos[raices[i]].frecuencia < nodos[raices[primero]].frecuencia) {
				segundo = primero;
				primero = i;

			} else if (nodos[raices[i]].frecuencia < nodos[raices[segundo]].frecuencia) {  
				segundo = i;
			}
		}

		int izquierda = raices[primero];
		int derecha = raices[segundo];

		// Crea un nuevo nodo combinando los dos nodos con las frecuencias más bajas y lo agrega a la lista de nodos
		nodos[cantidad] = (NodoHuffman){
			nodos[izquierda].frecuencia + nodos[derecha].frecuencia,
			izquierda, derecha, 0
		};

		raices[primero] = cantidad++;
		raices[segundo] = raices[--cantidad_raices];
	}

	return cantidad_raices == 0 ? -1 : raices[0];
}

// Lee el contenido de un archivo comprimido y lo carga en memoria.
int leer_archivo_comprimido(const char *ruta, ArchivoComprimido *archivo) {

	FILE *entrada = fopen(ruta, "rb");
	char magic[4];  // Array para almacenar el "magic number" del archivo comprimido
	uint32_t cantidad;

	// Inicializa la estructura de archivo comprimido a cero
	memset(archivo, 0, sizeof(*archivo));

	// Verifica que el archivo exista y tenga el formato esperado.
	if (entrada == NULL || leer_bytes(entrada, magic, sizeof(magic)) != 0 ||
		memcmp(magic, HUFFMAN_MAGIC, sizeof(magic)) != 0 ||
		leer_bytes(entrada, &cantidad, sizeof(cantidad)) != 0) {

		if (entrada != NULL) fclose(entrada);  // Cierra el archivo si se abrió correctamente antes de retornar un error
		return -1;
	}

	// Asigna memoria para los registros comprimidos en la estructura de archivo comprimido
	archivo->registros = calloc(cantidad, sizeof(*archivo->registros));

	if (cantidad != 0 && archivo->registros == NULL) {
		fclose(entrada);  // Cierra el archivo si no se pudo asignar memoria para los registros y retorna un error
		return -1;
	}

	archivo->cantidad = cantidad;  // Almacena la cantidad de registros en la estructura de archivo comprimido

	// Lee cada registro comprimido y lo guarda en la estructura.
	for (uint32_t i = 0; i < cantidad; ++i) {

		RegistroComprimido *registro = &archivo->registros[i];  // Puntero al registro comprimido actual

		uint32_t nombre_tamano;

		// Lee los metadatos y payload del registro actual.
		if (leer_bytes(entrada, &nombre_tamano, sizeof(nombre_tamano)) != 0 || nombre_tamano == 0 ||
			leer_bytes(entrada, &registro->tamano_original, sizeof(registro->tamano_original)) != 0 ||
			leer_bytes(entrada, &registro->bits, sizeof(registro->bits)) != 0 ||
			leer_bytes(entrada, registro->frecuencias, sizeof(registro->frecuencias)) != 0 ||
			
			registro->bits > UINT64_MAX - 7) {
			fclose(entrada);
			liberar_archivo_comprimido(archivo);

			return -1;
		}

		// Lee el hash MD5 del registro y lo valida.
		if (leer_bytes(entrada, registro->md5, 33) != 0) {
			fclose(entrada);
			liberar_archivo_comprimido(archivo);
			return -1;
		}

		// Asegura que el hash MD5 esté correctamente terminado con un carácter nulo
		registro->md5[32] = '\0';
		registro->nombre = malloc((size_t)nombre_tamano + 1);

		// Lee el nombre del archivo comprimido y lo almacena en la estructura de registro, asegurando que esté correctamente terminado con un carácter nulo
		if (registro->nombre == NULL || leer_bytes(entrada, registro->nombre, nombre_tamano) != 0) {
			fclose(entrada);
			liberar_archivo_comprimido(archivo);
			return -1;
		}

		// Asegura que el nombre del archivo termine en nulo.
		registro->nombre[nombre_tamano] = '\0';
		registro->datos_tamano = (size_t)((registro->bits + 7) / 8);
		registro->datos = malloc(registro->datos_tamano == 0 ? 1 : registro->datos_tamano);

		// Lee los datos comprimidos del archivo.
		if (registro->datos == NULL || leer_bytes(entrada, registro->datos, registro->datos_tamano) != 0) {
			fclose(entrada);
			liberar_archivo_comprimido(archivo);
			return -1;
		}

		// Actualiza el tamaño total del archivo comprimido en la estructura de archivo comprimido, sumando los tamaños de los campos leídos y los datos comprimidos
		archivo->tamano_comprimido += sizeof(nombre_tamano) + sizeof(registro->tamano_original) +
			sizeof(registro->bits) + sizeof(registro->frecuencias) + sizeof(registro->md5) +
			nombre_tamano + registro->datos_tamano;
	}

	fclose(entrada);
	archivo->tamano_comprimido += sizeof(magic) + sizeof(cantidad);
	return 0;
}

// Libera todos los registros y recursos del archivo comprimido.
void liberar_archivo_comprimido(ArchivoComprimido *archivo) {
	for (uint32_t i = 0; i < archivo->cantidad; ++i) liberar_registro(&archivo->registros[i]);
	free(archivo->registros);
	memset(archivo, 0, sizeof(*archivo));
}

// Descomprime un registro comprimido y lo guarda en el directorio indicado.
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

	// Si el tamaño original es cero, no hay nada que producir.
	if (registro->tamano_original == 0) {
		return fclose(salida) == 0 ? 0 : -1;
	}

	// Reconstruye el árbol Huffman a partir de las frecuencias guardadas.
	raiz = construir_arbol_descompresion(registro->frecuencias, nodos);

	if (raiz < 0) {
		fclose(salida);
		return -1;
	}

	// Si el árbol tiene un solo nodo, el símbolo se repite todas las veces necesarias.
	if (nodos[raiz].izquierda == -1) {
		for (uint64_t i = 0; i < registro->tamano_original; ++i) {

			// Escribe el símbolo del nodo raíz en el archivo de salida
			if (fputc(nodos[raiz].simbolo, salida) == EOF) {
				fclose(salida);
				return -1;
			}
		}

		return fclose(salida) == 0 ? 0 : -1;
	}

	nodo = raiz;

	// Recorre los bits comprimidos para reconstruir cada símbolo original.
	for (uint64_t bit = 0; bit < registro->bits && producidos < registro->tamano_original; ++bit) {
		unsigned char byte = registro->datos[bit / 8];
		nodo = ((byte >> (7 - (bit % 8))) & 1) == 0 ? nodos[nodo].izquierda : nodos[nodo].derecha;  // Determina si se debe ir a la izquierda o a la derecha en el árbol de Huffman según el valor del bit actual

		if (nodo < 0 || nodo >= MAX_NODOS_HUFFMAN) {
			fclose(salida);
			return -1;
		}

		// Cuando se alcanza una hoja, se escribe el símbolo y se reinicia desde la raíz.
		if (nodos[nodo].izquierda == -1) {
			if (fputc(nodos[nodo].simbolo, salida) == EOF) {
				fclose(salida);
				return -1;
			}
			++producidos;
			nodo = raiz;
		}
	}

	// Confirma que se generaron todos los bytes esperados y que el archivo cerró bien.
	if (producidos != registro->tamano_original || fclose(salida) != 0) {
		return -1;
	}

	return 0;
}
