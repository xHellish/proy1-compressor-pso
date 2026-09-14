#include <curl/curl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#define TOP_URL "https://www.gutenberg.org/browse/scores/top"
#define GUTENBERG_URL "https://www.gutenberg.org"
#define MAX_BOOKS 100

typedef struct {
	char *data;
	size_t size;
} Buffer;

static size_t write_to_buffer(void *contents, size_t size, size_t count,
							  void *user_data) {
	size_t bytes = size * count;
	Buffer *buffer = user_data;
	char *new_data = realloc(buffer->data, buffer->size + bytes + 1);

	if (new_data == NULL) {
		return 0;
	}

	buffer->data = new_data;
	memcpy(buffer->data + buffer->size, contents, bytes);
	buffer->size += bytes;
	buffer->data[buffer->size] = '\0';
	return bytes;
}

static int download_to_memory(const char *url, Buffer *buffer) {
	CURL *curl = curl_easy_init();
	CURLcode result;

	buffer->data = NULL;
	buffer->size = 0;
	if (curl == NULL) {
		return 0;
	}

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "gutenberg-top100-downloader/1.0");
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_to_buffer);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, buffer);
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 120L);

	result = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	if (result != CURLE_OK) {
		free(buffer->data);
		buffer->data = NULL;
		buffer->size = 0;
		return 0;
	}

	return 1;
}

static size_t extract_book_ids(const char *html, int *ids) {
	const char *section = strstr(html, "<h2 id=\"books-last30\">");
	const char *end;
	const char *cursor;
	size_t count = 0;

	if (section == NULL) {
		return 0;
	}

	end = strstr(section, "</ol>");
	if (end == NULL) {
		return 0;
	}

	cursor = section;
	while (cursor < end && count < MAX_BOOKS) {
		const char *link = strstr(cursor, "href=\"/ebooks/");
		int id;

		if (link == NULL || link >= end) {
			break;
		}

		if (sscanf(link + strlen("href=\"/ebooks/"), "%d", &id) == 1 &&
			id > 0) {
			int already_added = 0;
			size_t index;

			for (index = 0; index < count; ++index) {
				if (ids[index] == id) {
					already_added = 1;
					break;
				}
			}

			if (!already_added) {
				ids[count++] = id;
			}
		}

		cursor = link + 1;
	}

	return count;
}

static int find_plain_text_path(const char *html, int id, char *path,
								size_t path_size) {
	const char *cursor = html;
	char prefix[64];

	snprintf(prefix, sizeof(prefix), "href=\"/ebooks/%d.txt", id);
	while ((cursor = strstr(cursor, prefix)) != NULL) {
		const char *tag_end = strchr(cursor, '>');
		const char *quote;
		size_t path_length;

		if (tag_end == NULL) {
			return 0;
		}

		quote = strchr(cursor + 6, '"');
		if (quote == NULL || quote > tag_end) {
			cursor = tag_end + 1;
			continue;
		}

		if (strstr(cursor, "type=\"text/plain") == NULL ||
			strstr(cursor, "type=\"text/plain") > tag_end) {
			cursor = tag_end + 1;
			continue;
		}

		path_length = (size_t)(quote - (cursor + 6));
		if (path_length + 1 > path_size) {
			return 0;
		}

		memcpy(path, cursor + 6, path_length);
		path[path_length] = '\0';
		return 1;
	}

	return 0;
}

static int save_file(const char *filename, const char *data, size_t size) {
	FILE *file = fopen(filename, "wb");
	size_t written;

	if (file == NULL) {
		return 0;
	}

	written = fwrite(data, 1, size, file);
	fclose(file);
	return written == size;
}

int main(int argc, char **argv) {
	const char *output_dir = argc > 1 ? argv[1] : "gutenberg_txt";
	Buffer ranking;
	int ids[MAX_BOOKS];
	size_t book_count;
	size_t index;
	size_t downloaded = 0;

	if (mkdir(output_dir, 0755) != 0 && errno != EEXIST) {
		perror(output_dir);
		return EXIT_FAILURE;
	}

	if (!curl_global_init(CURL_GLOBAL_DEFAULT)) {
		if (!download_to_memory(TOP_URL, &ranking)) {
			fprintf(stderr, "No se pudo descargar el ranking de Gutenberg.\n");
			curl_global_cleanup();
			return EXIT_FAILURE;
		}
	} else {
		fprintf(stderr, "No se pudo inicializar libcurl.\n");
		return EXIT_FAILURE;
	}

	book_count = extract_book_ids(ranking.data, ids);
	free(ranking.data);
	if (book_count != MAX_BOOKS) {
		fprintf(stderr, "Se encontraron %zu libros en vez de %d.\n",
				book_count, MAX_BOOKS);
	}

	for (index = 0; index < book_count; ++index) {
		char page_url[128];
		char plain_path[256];
		char file_url[512];
		char filename[512];
		Buffer page;
		Buffer book;

		snprintf(page_url, sizeof(page_url), "%s/ebooks/%d", GUTENBERG_URL,
				 ids[index]);
		if (!download_to_memory(page_url, &page) ||
			!find_plain_text_path(page.data, ids[index], plain_path,
								  sizeof(plain_path))) {
			fprintf(stderr, "[%d] no tiene formato de texto plano.\n",
					ids[index]);
			free(page.data);
			continue;
		}
		free(page.data);

		snprintf(file_url, sizeof(file_url), "%s%s", GUTENBERG_URL,
				 plain_path);
		if (!download_to_memory(file_url, &book)) {
			fprintf(stderr, "[%d] no se pudo descargar %s.\n", ids[index],
					file_url);
			continue;
		}

		snprintf(filename, sizeof(filename), "%s/pg%d.txt", output_dir,
				 ids[index]);
		if (save_file(filename, book.data, book.size)) {
			printf("[%zu/%zu] descargado: %s\n", index + 1, book_count,
				   filename);
			++downloaded;
		} else {
			fprintf(stderr, "[%d] no se pudo guardar %s.\n", ids[index],
					filename);
		}
		free(book.data);
	}

	curl_global_cleanup();
	printf("Finalizado: %zu de %zu libros guardados en %s.\n", downloaded,
		   book_count, output_dir);
	return downloaded > 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
