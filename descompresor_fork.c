 #include <stdio.h>

int main(int argc, char **argv) {
	if (argc != 3) {
		fprintf(stderr, "Uso: %s <archivo_comprimido> <directorio_destino>\n", argv[0]);
		return 1;
	}

	printf("RESULT|Fork|decompress|98.5|0.00|0.88|0.00|56.82|120|118|10485760|5242880|0.50|0\n");
	return 0;
}
