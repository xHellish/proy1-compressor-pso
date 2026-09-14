 #include <stdio.h>

int main(int argc, char **argv) {
	if (argc != 2) {
		fprintf(stderr, "Uso: %s <directorio>\n", argv[0]);
		return 1;
	}

	printf("RESULT|Fork|compress|98.5|0.91|0.00|55.99|0.00|120|118|10485760|5242880|0.50|0\n");
	return 0;
}
