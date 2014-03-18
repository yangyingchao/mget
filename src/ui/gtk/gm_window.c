/** gm_window.c --- main window class
 *
 * Copyright (C) 2014 Yang,Ying-chao
 *
 * Author: Yang,Ying-chao <yangyingchao@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; see the file COPYING.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#define DEBUG       

#define _GNU_SOURCE         /* See feature_test_macros(7) */
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <gdk/gdkkeysyms.h>
#include "gm_window.h"
#include "mget_thread.h"
#include <libmget/mget_macros.h>

#define TAB_WIDTH_N_CHARS 15

extern const char* default_dir;

typedef struct _GmNewTaskDlg
{
    GtkDialog* gtk_dialog;
    GtkEntry*  entry_url;
    GtkEntry*  entry_dir;
    GtkEntry*  entry_fn;
} GmNewTaskDlg;

struct _GmWindowPriv {
    GtkWidget*  main_box;
    GtkBuilder* builder;
    GmNewTaskDlg new_task_dialog;
    GtkListStore* task_store;
    GtkTreeView* task_tree;
    GList* request_list;

    GtkToolButton* btn_add;
    GtkToolButton* btn_start;
    GtkToolButton* btn_pause;
    GtkToolButton* btn_settings;
};

enum {
    UPDATE_PROGRESS,
    LAST_SIGNAL
};

typedef enum _task_status
{
    TS_PAUSED = 0,
    TS_ON_GOING,
    TS_FINISHED,
} task_status;


void pause_request(gpointer data, gpointer user_data)
{
    PDEBUG ("data: %p\n", data);

    gmget_request* req = (gmget_request*)data;
    if (req)
    {
        req->request.flag = true;
        (*(gint*)user_data)++;
    }
}

void  on_box1_destroy (GtkWidget *widget,
                           gpointer   user_data)
{
    if (!user_data)
    {
        return;
    }

    GmWindow* window = (GmWindow*) user_data;
    gint counter = 0;
    g_list_foreach(window->priv->request_list, pause_request, &counter);
    if (counter)
    {
        sleep(3);
    }
    sync();
}


static gint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GmWindow, gm_window, GTK_TYPE_APPLICATION_WINDOW);

#define GET_PRIVATE(instance) G_TYPE_INSTANCE_GET_PRIVATE   \
    (instance, GM_TYPE_WINDOW, GmWindowPriv);


#define show_iter(X)       printf("ITER: %u, %p, %p, %p\n",     \
                                  (X)->stamp, (X)->user_data,   \
                                  (X)->user_data2,              \
                                  (X)->user_data3)

#define G_SHOW(X)  gtk_widget_show_all ((GtkWidget*)(X));
#define G_HIDE(X)  gtk_widget_hide ((GtkWidget*)(X));

#define G_ENABLE_WIDGET(X, Y) gtk_widget_set_sensitive((GtkWidget*)(X), (Y))


void on_btn_settings_clicked(GtkButton *button,
                             gpointer   user_data)
{
    if (!user_data)
    {
        return;
    }

    GmWindow* window = (GmWindow*)user_data;
    GtkWidget* dlg = G_GET_WIDGET(window->priv->builder, GtkWidget,
                                  "dlg_settings");
    if (dlg)
    {
        G_SHOW(dlg);
    }
}

void on_btn_setting_ok_clicked(GtkButton *button,
                               gpointer   user_data)
{
    if (!user_data)
    {
        return;
    }

    GmWindow* window = (GmWindow*)user_data;
    GtkWidget* dlg = G_GET_WIDGET(window->priv->builder, GtkWidget,
                                  "dlg_settings");
    if (dlg)
    {
        G_HIDE(dlg);
    }
}

void on_btn_setting_cancel_clicked(GtkButton *button,
                                   gpointer   user_data)
{
    on_btn_setting_ok_clicked(button, user_data);
}

void on_btn_add_clicked(GtkButton *button,
                        gpointer   user_data)
{
    printf ("hello!\n");
    if (!user_data)
    {
        return;
    }

    GmWindow* window = (GmWindow*) user_data;
    G_SHOW (window->priv->new_task_dialog.gtk_dialog);
}

void on_btn_download_cancel_clicked(GtkButton *button,
                                    gpointer   user_data)
{
    GtkWidget* dlg = (GtkWidget*)user_data;
    gtk_widget_hide (dlg);
}

void on_btn_download_confirm_clicked(GtkButton *button,
                                     gpointer   user_data)
{
    if (!user_data)
    {
        return;
    }

    GmWindow* window = (GmWindow*)user_data;
    GmNewTaskDlg* dlg = &(window->priv->new_task_dialog);
    const gchar* url = gtk_entry_get_text(dlg->entry_url);
    if (!url || !strlen(url))
    {
        goto out;
    }
    gmget_request* g_request = ZALLOC1(gmget_request);

    g_request->request.url = g_strdup(url);
    gtk_entry_set_text (dlg->entry_url, "");

    g_request->window = window;
    const gchar* dir = gtk_entry_get_text(dlg->entry_dir);
    if (dir && strlen(dir))
    {
        g_request->request.fn.dirn = g_strdup(dir);
    }
    else if (!access(default_dir, F_OK))
    {
        g_request->request.fn.dirn = g_strdup(default_dir);
    }
    gtk_entry_set_text (dlg->entry_dir, "");

    const gchar* fn = gtk_entry_get_text(dlg->entry_fn);
    if (fn && strlen(fn))
    {
        g_request->request.fn.basen = g_strdup(fn);
    }
    gtk_entry_set_text (dlg->entry_fn, "");
    g_request->request.nc = 10;
    bool v = false;
    GtkListStore* task_store =  window->priv->task_store;

    GtkTreeIter* iter = &(g_request->iter);
    gtk_list_store_append(task_store, iter);
    gtk_list_store_set(task_store, iter, 1, url, 2, "Unkown",
                       3, (double)0, -1);
    /* int r = start_request(url, &g_request->request.fn, nc, update_progress, &v); */
    /* printf ("r = %d\n", r); */
    window->priv->request_list = g_list_append(window->priv->request_list,
                                               g_request);
    pthread_t* tid = start_request_thread(g_request);
    //@todo:
    // 1. Make hash table to store request.
    // 2. Start threads to handle g_request.

out:
    G_HIDE(window->priv->new_task_dialog.gtk_dialog);
}


static void
window_update_progress_cb (void       *window,
                           const char *location,
                           const char  *name,
                           const char  *size,
                           gpointer    user_data,
                           double      percentage,
                           const char* speed,
                           const char* eta)
{
    GmWindow* g_window = (GmWindow*)window;
    char* Bps = NULL;
    asprintf(&Bps, "%s/s", speed);
    gtk_list_store_set(g_window->priv->task_store,
                       (GtkTreeIter*)user_data,
                       1, name,
                       2, size,
                       3, percentage,
                       4, Bps,
                       5, eta,
                       -1);
    free(Bps);
}

typedef enum _toolbar_button_status
{
    TBS_CAN_ADD   = 1,
    TBS_CAN_START = 1 << 1,
    TBS_CAN_PAUSE = 1 <<2,
} toolbar_button_status;

// check selected row function paramter
typedef struct _csrf_param
{
    GtkTreeModel* model;
    gint          status;
} csrf_param;

static void check_selected_row(gpointer data, gpointer user_data)
{
    csrf_param* param = (csrf_param*)user_data;
    GtkTreePath* path = (GtkTreePath*)data;
    GtkTreeIter  iter;

    if (!path || !param || !param->model ||
        !gtk_tree_model_get_iter(param->model, &iter, path))
    {
        return;
    }

    gchar* str = NULL;
    gtk_tree_model_get(param->model, &iter, 0, &str, -1);
    int status = str ? atoi(str) : TBS_CAN_ADD;
    switch (status)
    {
        case TS_PAUSED:
        {
            param->status |= TBS_CAN_START;
            break;
        }
        case TS_ON_GOING:
        {
            param->status |= TBS_CAN_PAUSE;
            break;
        }
        default:
        {
            break;
        }
    }
}

/* Prototype for selection handler callback */
static void
tree_selection_changed_cb (GtkTreeSelection *selection, gpointer data)
{
    GmWindow*     window = (GmWindow*)data;
    GtkTreeModel *model  = NULL;
    GList* selected_rows =
            gtk_tree_selection_get_selected_rows(selection,
                                                 &model);
    csrf_param* param = ZALLOC1(csrf_param);
    param->model = model;

    g_list_foreach(selected_rows, check_selected_row, param);

    PDEBUG ("status: %d\n", param->status);

    G_ENABLE_WIDGET(window->priv->btn_start,
                    (param->status & TBS_CAN_START) ? TRUE : FALSE);

    G_ENABLE_WIDGET(window->priv->btn_pause,
                    (param->status & TBS_CAN_PAUSE) ? TRUE : FALSE);

    g_list_free_full (selected_rows, (GDestroyNotify) gtk_tree_path_free);
}

static void
gm_window_init (GmWindow *window)
{
    GmWindowPriv  *priv;
    GtkAccelGroup *accel_group;
    GClosure      *closure;
    gint           i;
    GError        *error = NULL;

    gtk_window_set_default_size((GtkWindow*)window, 800, 600);

    priv = GET_PRIVATE (window);
    window->priv = priv;

    gtk_window_set_hide_titlebar_when_maximized (GTK_WINDOW (window), TRUE);

    /* priv->selected_search_link = NULL; */

    /* /\* handle settings *\/ */
    /* priv->settings = gm_settings_get (); */
    /* priv->fonts_changed_id = g_signal_connect (priv->settings, */
    /*                                            "fonts-changed", */
    /*                                            G_CALLBACK (settings_fonts_changed_cb), */
    /*                                            window); */

    /* Setup builder */
    GtkBuilder* builder = gtk_builder_new ();
    /* if (!gtk_builder_add_from_resource (priv->builder,  "/org/gnome/devhelp/devhelp.ui", &error)) */
    if (!gtk_builder_add_from_file (builder,  "res/gmget.glade", &error))
    {
        g_error ("Cannot add resource to builder: %s", error ? error->message : "unknown error");
        g_clear_error (&error);
    }

    priv->builder = builder;
    priv->main_box = G_GET_WIDGET(builder, GtkWidget, "box1");
    gtk_widget_show (priv->main_box);
    gtk_container_add (GTK_CONTAINER (window), priv->main_box);

    GmNewTaskDlg* dlg = &priv->new_task_dialog;
    dlg->gtk_dialog = G_GET_WIDGET(priv->builder, GtkDialog, "dlg_new_task");
    dlg->entry_url  = G_GET_WIDGET(builder, GtkEntry, "entry_url");
    dlg->entry_dir  = G_GET_WIDGET(builder, GtkEntry, "entry_dir");
    dlg->entry_fn   = G_GET_WIDGET(builder, GtkEntry, "entry_filename");

    priv->request_list = g_list_alloc();
    window->priv->task_tree = G_GET_WIDGET(window->priv->builder,
                                           GtkTreeView, "task_tree");

    GtkListStore* task_store = G_GET_WIDGET(window->priv->builder,
                                            GtkListStore, "liststore_tasks");
    window->priv->task_store = task_store;
    /* Setup the selection handler */
    GtkTreeSelection *select =
            gtk_tree_view_get_selection (GTK_TREE_VIEW
                                         (window->priv->task_tree));
    gtk_tree_selection_set_mode (select, GTK_SELECTION_MULTIPLE);

    g_signal_connect (G_OBJECT (select), "changed",
                      G_CALLBACK (tree_selection_changed_cb),
                      window);
    g_signal_connect (window, "update-progress", G_CALLBACK
                      (window_update_progress_cb), window);


    window->priv->btn_add = G_GET_WIDGET(window->priv->builder,
                                         GtkToolButton,
                                         "btn_new");
    window->priv->btn_start = G_GET_WIDGET(window->priv->builder,
                                           GtkToolButton,
                                           "btn_start");
    window->priv->btn_pause = G_GET_WIDGET(window->priv->builder,
                                           GtkToolButton,
                                           "btn_pause");
    window->priv->btn_settings =G_GET_WIDGET(window->priv->builder,
                                             GtkToolButton,
                                             "btn_settings");

    /* g_action_map_add_action_entries (G_ACTION_MAP (window), */
    /*                                  win_entries, G_N_ELEMENTS (win_entries), */
    /*                                  window); */

    /* accel_group = gtk_accel_group_new (); */
    /* gtk_window_add_accel_group (GTK_WINDOW (window), accel_group); */

    gtk_builder_connect_signals(priv->builder, window);
}

static void
dispose (GObject *object)
{
	GmWindow *self = GM_WINDOW (object);

    /* if (self->priv->fonts_changed_id) { */
    /*     if (self->priv->settings && g_signal_handler_is_connected (self->priv->settings, self->priv->fonts_changed_id)) */
    /*         g_signal_handler_disconnect (self->priv->settings, self->priv->fonts_changed_id); */
    /*     self->priv->fonts_changed_id = 0; */
    /* } */

    /* g_clear_object (&self->priv->settings); */
    /* g_clear_object (&self->priv->builder); */

	/* Chain up to the parent class */
	G_OBJECT_CLASS (gm_window_parent_class)->dispose (object);
}

static void
gm_window_class_init (GmWindowClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    g_type_class_add_private (klass, sizeof (GmWindowPriv));
    object_class->dispose = dispose;

    signals[UPDATE_PROGRESS] =
            g_signal_new ("update-progress",
                          G_TYPE_FROM_CLASS (klass),
                          G_SIGNAL_RUN_LAST,
                          G_STRUCT_OFFSET (GmWindowClass, update_progress),
                          NULL, NULL,
                          g_cclosure_marshal_generic,
                          G_TYPE_NONE,
                          7,
                          G_TYPE_STRING,
                          G_TYPE_STRING,
                          G_TYPE_STRING,
                          G_TYPE_POINTER,
                          G_TYPE_DOUBLE,
                          G_TYPE_STRING,
                          G_TYPE_STRING);
}

GtkWidget *
gm_window_new (GmApp *application)
{
    GmWindow     *window;
    GmWindowPriv *priv;

    window = g_object_new (GM_TYPE_WINDOW, NULL);
    priv = window->priv;

    gtk_window_set_application (GTK_WINDOW (window), GTK_APPLICATION (application));

    /* window_populate (window); */

    /* gtk_window_set_icon_name (GTK_WINDOW (window), "gmget"); */
    /* gtk_window_set_default_icon_from_file (GTK_WINDOW (window),"/mnt/data/Work/mget/src/ui/gtk/res/gmget.png"); */


    /* g_signal_connect (window, "configure-event", */
    /*                   G_CALLBACK (window_configure_event_cb), */
    /*                   window); */

    /* gm_util_window_settings_restore ( */
    /*     GTK_WINDOW (window), */
    /*     gm_settings_peek_window_settings (priv->settings), TRUE); */

    /* g_settings_bind (gm_settings_peek_paned_settings (priv->settings), */
    /*                  "position", G_OBJECT (priv->hpaned), */
    /*                  "position", G_SETTINGS_BIND_DEFAULT); */

    return GTK_WIDGET (window);
}
