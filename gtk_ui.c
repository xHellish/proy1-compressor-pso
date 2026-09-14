#include "gtk_ui.h"

static void actualizar_estado(AppState *state, const gchar *message) {
	gtk_label_set_text(GTK_LABEL(state->status_label), message);
}

static gint indice_metodo(const gchar *metodo) {
	if (g_strcmp0(metodo, "Normal") == 0) {
		return 0;
	}
	if (g_strcmp0(metodo, "Fork") == 0) {
		return 1;
	}
	if (g_strcmp0(metodo, "Pthread") == 0) {
		return 2;
	}
	return -1;
}

static void actualizar_resultado(AppState *state, gchar **parts) {
	gint row = indice_metodo(parts[1]);
	gchar *values[8];

	if (row < 0 || g_strv_length(parts) < 14) {
		return;
	}

	values[0] = g_strdup_printf("%s%%", parts[3]);
	values[1] = g_strdup_printf("%s s", parts[4]);
	values[2] = g_strdup_printf("%s s", parts[5]);
	values[3] = g_strdup_printf("%s%%", parts[6]);
	values[4] = g_strdup_printf("%s%%", parts[7]);
	values[5] = g_strdup(parts[10]);
	values[6] = g_strdup(parts[11]);
	values[7] = g_strdup(parts[12]);

	for (guint column = 0; column < G_N_ELEMENTS(values); column++) {
		gtk_label_set_text(GTK_LABEL(state->result_cells[row][column]), values[column]);
		g_free(values[column]);
	}
}

static void resultado_recibido(GObject *source_object,
							   GAsyncResult *async_result,
							   gpointer user_data) {
	AppState *state = user_data;
	GSubprocess *process = G_SUBPROCESS(source_object);
	gchar *stdout_text = NULL;
	gchar *stderr_text = NULL;
	GError *error = NULL;

	if (!g_subprocess_communicate_utf8_finish(process, async_result,
											 &stdout_text, &stderr_text, &error)) {
		actualizar_estado(state, error->message);
		g_clear_error(&error);
		g_free(stdout_text);
		g_free(stderr_text);
		return;
	}

	g_strstrip(stdout_text);
	if (g_str_has_prefix(stdout_text, "RESULT|")) {
		gchar **parts = g_strsplit(stdout_text, "|", -1);
		actualizar_resultado(state, parts);
		g_strfreev(parts);
		actualizar_estado(state, "Resultado recibido y tabla actualizada.");
	} else {
		actualizar_estado(state, "El proceso no devolvió un resultado válido.");
	}

	g_free(stdout_text);
	g_free(stderr_text);
}

static void seleccionar_directorio_finalizado(GObject *source_object,
											  GAsyncResult *result,
											  gpointer user_data) {
	AppState *state = user_data;
	GtkFileDialog *dialog = GTK_FILE_DIALOG(source_object);
	GError *error = NULL;
	GFile *directory = gtk_file_dialog_select_folder_finish(dialog, result, &error);

	if (directory == NULL) {
		if (error != NULL && !g_error_matches(error, GTK_DIALOG_ERROR, GTK_DIALOG_ERROR_DISMISSED)) {
			actualizar_estado(state, error->message);
		}
		g_clear_error(&error);
		g_object_unref(dialog);
		return;
	}

	g_clear_pointer(&state->directory, g_free);
	state->directory = g_file_get_path(directory);
	gtk_label_set_text(GTK_LABEL(state->directory_label), state->directory);
	actualizar_estado(state, "Directorio seleccionado. Elige una operación.");

	g_object_unref(directory);
	g_object_unref(dialog);
}

static void seleccionar_directorio(GtkButton *button, gpointer user_data) {
	AppState *state = user_data;
	GtkFileDialog *dialog = gtk_file_dialog_new();

	(void)button;
	gtk_file_dialog_set_title(dialog, "Seleccionar directorio");
	gtk_file_dialog_select_folder(dialog, GTK_WINDOW(state->window), NULL,
								  seleccionar_directorio_finalizado, state);
}

static void ejecutar_todas(GtkButton *button, gpointer user_data) {
	AppState *state = user_data;
	const gchar *operation = g_object_get_data(G_OBJECT(button), "operation");
	const gchar *programs[3];
	guint started = 0;
	guint missing = 0;

	if (state->directory == NULL) {
		actualizar_estado(state, "Selecciona un directorio antes de continuar.");
		return;
	}

	if (g_strcmp0(operation, "compress") == 0) {
		programs[0] = "./compresor_normal";
		programs[1] = "./compresor_fork";
		programs[2] = "./compresor_pthread";
	} else {
		programs[0] = "./descompresor_normal";
		programs[1] = "./descompresor_fork";
		programs[2] = "./descompresor_pthread";
	}

	for (guint index = 0; index < G_N_ELEMENTS(programs); index++) {
		if (!g_file_test(programs[index], G_FILE_TEST_IS_EXECUTABLE)) {
			missing++;
			continue;
		}

		GError *error = NULL;
		GSubprocess *process = g_subprocess_new(
			G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
			&error, programs[index], state->directory, NULL);
		if (process != NULL) {
			started++;
			g_subprocess_communicate_utf8_async(process, NULL, NULL,
										 resultado_recibido, state);
			g_object_unref(process);
		}
		g_clear_error(&error);
	}

	if (missing > 0) {
		gchar *message = g_strdup_printf("%u programa(s) no disponible(s). Se muestran placeholders.", missing);
		actualizar_estado(state, message);
		g_free(message);
	} else {
		gchar *message = g_strdup_printf("Se iniciaron %u corridas. Las estadísticas se actualizarán cuando existan resultados.", started);
		actualizar_estado(state, message);
		g_free(message);
	}
}

static GtkWidget *crear_boton_operacion(const gchar *label,
										const gchar *operation,
										AppState *state) {
	GtkWidget *button = gtk_button_new_with_label(label);
	g_object_set_data_full(G_OBJECT(button), "operation", g_strdup(operation), g_free);
	g_signal_connect(button, "clicked", G_CALLBACK(ejecutar_todas), state);
	return button;
}

static GtkWidget *agregar_celda(GtkGrid *grid, const gchar *text, gint column, gint row) {
	GtkWidget *label = gtk_label_new(text);
	gtk_label_set_wrap(GTK_LABEL(label), TRUE);
	gtk_widget_set_halign(label, GTK_ALIGN_START);
	gtk_widget_set_margin_top(label, 6);
	gtk_widget_set_margin_bottom(label, 6);
	gtk_widget_set_margin_start(label, 8);
	gtk_widget_set_margin_end(label, 8);
	gtk_grid_attach(grid, label, column, row, 1, 1);
	return label;
}

static GtkWidget *crear_tabla_estadisticas(AppState *state) {
	static const gchar *headers[] = {
		"Método", "Salud (%)", "Tiempo comp.", "Tiempo descomp.",
		"Aceleración comp. (%)", "Aceleración descomp. (%)",
		"Originales", "Comprimido", "Radio"
	};
	static const gchar *methods[] = {"Normal", "Fork", "Pthread"};
	GtkWidget *grid = gtk_grid_new();

	gtk_grid_set_row_spacing(GTK_GRID(grid), 2);
	gtk_grid_set_column_spacing(GTK_GRID(grid), 2);
	for (guint column = 0; column < G_N_ELEMENTS(headers); column++) {
		agregar_celda(GTK_GRID(grid), headers[column], column, 0);
	}
	for (guint row = 0; row < G_N_ELEMENTS(methods); row++) {
		agregar_celda(GTK_GRID(grid), methods[row], 0, row + 1);
		for (guint column = 1; column < G_N_ELEMENTS(headers); column++) {
			state->result_cells[row][column - 1] = agregar_celda(
				GTK_GRID(grid), "Pendiente", column, row + 1);
		}
	}
	return grid;
}

void activar_ui(GtkApplication *app) {
	AppState *state = g_new0(AppState, 1);
	GtkWidget *window = gtk_application_window_new(app);
	GtkWidget *main_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 18);
	GtkWidget *directory_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
	GtkWidget *title = gtk_label_new("Compresor PSO");
	GtkWidget *description = gtk_label_new("Selecciona un directorio y ejecuta las tres variantes de cada operación.");
	GtkWidget *choose_button = gtk_button_new_with_label("Navegar...");
	GtkWidget *directory_label = gtk_label_new("Ningún directorio seleccionado");
	GtkWidget *operation_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
	GtkWidget *statistics_title = gtk_label_new("Tabla comparativa (placeholders)");
	GtkWidget *statistics_scroll = gtk_scrolled_window_new();
	GtkWidget *statistics_table = crear_tabla_estadisticas(state);
	GtkWidget *status_label = gtk_label_new("Selecciona un directorio para comenzar.");

	state->window = window;
	state->directory_label = directory_label;
	state->status_label = status_label;

	gtk_window_set_title(GTK_WINDOW(window), "Compresor PSO");
	gtk_window_set_default_size(GTK_WINDOW(window), 1280, 720);
	gtk_window_set_child(GTK_WINDOW(window), main_box);
	gtk_widget_set_margin_top(main_box, 28);
	gtk_widget_set_margin_bottom(main_box, 28);
	gtk_widget_set_margin_start(main_box, 32);
	gtk_widget_set_margin_end(main_box, 32);

	gtk_widget_set_halign(title, GTK_ALIGN_START);
	gtk_widget_set_halign(description, GTK_ALIGN_START);
	gtk_widget_set_halign(directory_label, GTK_ALIGN_START);
	gtk_widget_set_halign(status_label, GTK_ALIGN_START);
	gtk_widget_set_halign(statistics_title, GTK_ALIGN_START);
	gtk_label_set_wrap(GTK_LABEL(description), TRUE);
	gtk_label_set_wrap(GTK_LABEL(status_label), TRUE);
	gtk_widget_set_hexpand(directory_label, TRUE);
	gtk_widget_set_hexpand(status_label, TRUE);
	gtk_widget_set_margin_top(status_label, 18);

	gtk_box_append(GTK_BOX(main_box), title);
	gtk_box_append(GTK_BOX(main_box), description);
	gtk_box_append(GTK_BOX(main_box), directory_box);
	gtk_box_append(GTK_BOX(directory_box), directory_label);
	gtk_box_append(GTK_BOX(directory_box), choose_button);
	gtk_box_append(GTK_BOX(main_box), operation_box);
	gtk_box_append(GTK_BOX(operation_box), crear_boton_operacion("Comprimir", "compress", state));
	gtk_box_append(GTK_BOX(operation_box), crear_boton_operacion("Descomprimir", "decompress", state));
	gtk_box_append(GTK_BOX(main_box), statistics_title);
	gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(statistics_scroll), statistics_table);
	gtk_widget_set_vexpand(statistics_scroll, TRUE);
	gtk_box_append(GTK_BOX(main_box), statistics_scroll);
	gtk_box_append(GTK_BOX(main_box), status_label);

	g_signal_connect(choose_button, "clicked", G_CALLBACK(seleccionar_directorio), state);
	g_object_set_data_full(G_OBJECT(window), "state", state, (GDestroyNotify)g_free);
	gtk_window_present(GTK_WINDOW(window));
}
