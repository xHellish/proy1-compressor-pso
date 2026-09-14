#include "gtk_ui.h"

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("org.ejemplo.CompresorPSO", G_APPLICATION_DEFAULT_FLAGS);
    int status;

    g_signal_connect(app, "activate", G_CALLBACK(activar_ui), NULL);
    status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}