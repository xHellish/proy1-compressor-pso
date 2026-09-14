#ifndef GTK_UI_H
#define GTK_UI_H

#include <gtk/gtk.h>

typedef struct {
	gchar *metodo;
	gchar *operacion;
	gchar *estado;
	gdouble salud;
	gdouble tiempo_compresion;
	gdouble tiempo_descompresion;
	gdouble aceleracion_compresion;
	gdouble aceleracion_descompresion;
	guint64 cantidad_archivos;
	guint64 firmas_verificadas;
	guint64 tamano_original;
	guint64 tamano_comprimido;
	gdouble radio_compresion;
	gint codigo_salida;
} Resultado;

typedef struct {
	GtkWidget *window;
	GtkWidget *source_label;
	GtkWidget *archive_label;
	GtkWidget *destination_label;
	GtkWidget *status_label;
	GtkWidget *result_cells[3][8];
	gchar *source_directory;
	gchar *archive_file;
	gchar *destination_directory;
	Resultado resultados[3];
} AppState;

void activar_ui(GtkApplication *app);

#endif
