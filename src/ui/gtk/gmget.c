#include <gtk/gtk.h>
#include <libmget/libmget.h>
#include <libmget/mget_macros.h>
#include "mget_thread.h"
#include <stdlib.h>
#include <string.h>

static GtkBuilder *g_builder = NULL;


static int nc = 10;

#define show_iter(X)       printf("ITER: %u, %p, %p, %p\n", \
                                  (X)->stamp, (X)->user_data, \
                                  (X)->user_data2,            \
                                  (X)->user_data3)


void update_progress(metadata * md)
{
    static int idx = 0;
    static uint32 ts = 0;
    static uint64 last_recv = 0;

    if (md->hd.status == RS_SUCCEEDED) {
        char *date = current_time_str();

        printf("%s - %s saved in %s [%.02fKB/s] ...\n",
               date, md->fn,
               stringify_time(md->hd.acc_time),
               (double) (md->hd.package_size) / K / md->hd.acc_time);
        free(date);
        return;
    }

    int threshhold = 78 * md->hd.acon / md->hd.nr_effective;
    if (idx < threshhold) {
        if (!ts) {
            ts = get_time_ms();
            fprintf(stderr, ".");
        } else if ((get_time_ms() - ts) > 1000 / threshhold * idx) {
            fprintf(stderr, ".");
            idx++;
        }
    } else {
        data_chunk *dp = md->body;
        uint64 total = md->hd.package_size;
        uint64 recv = 0;

        for (int i = 0; i < md->hd.nr_effective; ++i) {
            recv += dp->cur_pos - dp->start_pos;
            dp++;
        }

        uint64 diff_size = recv - last_recv;
        uint64 remain = total - recv;
        uint32 c_time = get_time_ms();
        uint64 bps =
            (uint64) ((double) (diff_size) * 1000 / (c_time - ts));

        fprintf(stderr,
                "] %.02f percent finished, speed: %s/s, eta: %s\r",
                (double) recv / total * 100,
                stringify_size(bps), stringify_time((total - recv) / bps));
        fprintf(stderr, "\n");

        idx = 0;
        last_recv = recv;
        ts = c_time;
    }
}

#define G_GET_WIDGET(T, N)   \
    (T*)(gtk_builder_get_object(g_builder, N))

gboolean on_MainWindow_destroy(GtkWidget * widget,
                               GdkEvent * event, gpointer user_data)
{
    gtk_widget_destroy(widget);
    gtk_main_quit();
}

void on_btn_add_clicked(GtkButton * button, gpointer user_data)
{
    printf("hello!\n");
    GtkWidget *dlg = (GtkWidget *) user_data;
    gtk_widget_show_all(dlg);
}

void on_btn_download_cancel_clicked(GtkButton * button, gpointer user_data)
{
    GtkWidget *dlg = (GtkWidget *) user_data;
    gtk_widget_hide(dlg);
}

void on_btn_download_confirm_clicked(GtkButton * button,
                                     gpointer user_data)
{
    GtkWidget *dlg = (GtkWidget *) user_data;

    GtkEntry *entry_url = G_GET_WIDGET(GtkEntry, "entry_url");
    const gchar *url = gtk_entry_get_text(entry_url);
    if (!url || !strlen(url)) {
        goto out;
    }

    gmet_request *g_request = ZALLOC1(gmet_request);
    GtkEntry *entry_dir = G_GET_WIDGET(GtkEntry, "entry_dir");
    const gchar *dir = gtk_entry_get_text(entry_dir);
    if (dir && strlen(dir)) {
        g_request->request.fn.dirn = g_strdup(dir);
    }

    GtkEntry *entry_fn = G_GET_WIDGET(GtkEntry, "entry_filename");
    const gchar *fn = gtk_entry_get_text(entry_fn);
    if (fn && strlen(fn)) {
        g_request->request.fn.basen = g_strdup(fn);
    }

    printf("url: %s, dir: %s, fn: %s\n", url, dir, fn);

    bool v = false;

    GtkListStore *store_tasks =
        G_GET_WIDGET(GtkListStore, "liststore_tasks");
    printf("soter: %p\n", store_tasks);
    GtkTreeIter *iter = &(g_request->iter);
    gtk_list_store_append(store_tasks, iter);
    show_iter(iter);
    gtk_list_store_set(store_tasks, iter, 1, url, 2, "Unkown",
                       3, (double) 0, -1);
    /* int r = start_request(url, &g_request->request.fn, nc, update_progress, &v); */
    /* printf ("r = %d\n", r); */

    //@todo:
    // 1. Make hash table to store request.
    // 2. Start threads to handle g_request.


  out:
    gtk_widget_hide(dlg);
}

int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);
    g_builder = gtk_builder_new();

    guint ret =
        gtk_builder_add_from_file(g_builder, "res/gmget.glade", NULL);
    if (!ret) {
        printf("Failed to parse glade file!\n");
        return 1;
    }

    GtkWidget *window = G_GET_WIDGET(GtkWidget, "MainWindow");
    if (!window) {
        printf("Failed to get window!\n");
        return -1;
    }

    /* g_signal_connect (window, "destroy", G_CALLBACK (on_window_destroy), NULL); */
    gtk_builder_connect_signals(g_builder, NULL);
    /* g_object_unref(g_builder); */

    gtk_widget_show(window);

    gtk_main();
    return 0;
}
