 #include <stdio.h>

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Uso: %s <directorio>\n", argv[0]);
		return 1;
	}

	printf("RESULT|Normal|decompress|98.5|0.00|1.38|0.00|0.00|120|118|10485760|5242880|0.50|0\n");
	return 0;
}
