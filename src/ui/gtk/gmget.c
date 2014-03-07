#include <gtk/gtk.h>

void  on_window_destroy(GtkWidget* object, gpointer user_data)
{
    gtk_widget_destroy(object);
    gtk_main_quit();
 }

void on_btn_add_clicked(GtkButton *button,
                        gpointer   user_data)
{
    printf ("hello!\n");
    GtkWidget* dlg = (GtkWidget*)user_data;
    gtk_widget_show_all (dlg);
}

void on_btn_download_cancel_clicked(GtkButton *button,
                                    gpointer   user_data)
{
    printf ("hello!\n");
    GtkWidget* dlg = (GtkWidget*)user_data;
    gtk_widget_hide (dlg);
}

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);
    GtkBuilder *builder = gtk_builder_new();

    guint ret = gtk_builder_add_from_file(builder, "res/gmget.glade", NULL);
    if (!ret)
    {
        printf ("Failed to parse glade file!\n");
        return 1;
    }

    GtkWidget* window = GTK_WIDGET(gtk_builder_get_object(builder,
                                                          "MainWindow"));
    if (!window)
    {
        printf ("Failed to get window!\n");
        return -1;
    }

    /* g_signal_connect (window, "destroy", G_CALLBACK (on_window_destroy), NULL); */
    gtk_builder_connect_signals(builder, NULL);
    g_object_unref(builder);

    gtk_widget_show(window);

    gtk_main();
    return 0;
}
