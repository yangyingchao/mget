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


#include <string.h>
#include <math.h>
#include <gdk/gdkkeysyms.h>
#include "gm_window.h"

#define TAB_WIDTH_N_CHARS 15

struct _GmWindowPriv {
    GtkBox      *main_box;
    GtkBuilder* builder;
};

enum {
    UPDATE_PROGRESS,
    LAST_SIGNAL
};

static gint signals[LAST_SIGNAL] = { 0 };

G_DEFINE_TYPE (GmWindow, gm_window, GTK_TYPE_APPLICATION_WINDOW);

#define GET_PRIVATE(instance) G_TYPE_INSTANCE_GET_PRIVATE   \
    (instance, GM_TYPE_WINDOW, GmWindowPriv);

static void
window_update_progress_cb (GmWindow *window,
                           const char *location,
                           GmOpenLinkFlags flags)
{
}

static void
gm_window_init (GmWindow *window)
{
    GmWindowPriv  *priv;
    GtkAccelGroup *accel_group;
    GClosure      *closure;
    gint           i;
    GError        *error = NULL;

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
    priv->builder = gtk_builder_new ();
    /* if (!gtk_builder_add_from_resource (priv->builder,  "/org/gnome/devhelp/devhelp.ui", &error)) */
    if (!gtk_builder_add_from_file (priv->builder,  "res/gmget.glade", &error))
    {
        g_error ("Cannot add resource to builder: %s", error ? error->message : "unknown error");
        g_clear_error (&error);
    }

    priv->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
    /* gtk_widget_show (priv->main_box); */

    gtk_container_add (GTK_CONTAINER (window), G_GET_WIDGET(priv->builder, GtkWidget, "MainWindow"));
    g_signal_connect (window,
                      "update-progress",
                      G_CALLBACK (window_update_progress_cb),
                      window);

    /* g_action_map_add_action_entries (G_ACTION_MAP (window), */
    /*                                  win_entries, G_N_ELEMENTS (win_entries), */
    /*                                  window); */

    accel_group = gtk_accel_group_new ();
    gtk_window_add_accel_group (GTK_WINDOW (window), accel_group);
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
                          2,
                          G_TYPE_STRING,
                          G_TYPE_DOUBLE);
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

    gtk_window_set_icon_name (GTK_WINDOW (window), "devhelp");

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
