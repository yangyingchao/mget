/*
 * Copyright (C) 2002 CodeFactory AB
 * Copyright (C) 2002 Mikael Hallendal <micke@imendio.com>
 * Copyright (C) 2004-2008 Imendio AB
 * Copyright (C) 2012 Aleksander Morgado <aleksander@gnu.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <glib/gi18n.h>

#include "gm-app.h"
#include "gm_window.h"

typedef struct _GmBookManager
{
    int a;
} GmBookManager;

struct _GmAppPrivate {
    GmBookManager *book_manager;
    GtkBuilder* menu_builder;
};

const char* default_dir = "/tmp/";

G_DEFINE_TYPE (GmApp, gm_app, GTK_TYPE_APPLICATION);

/******************************************************************************/

GmBookManager *
gm_app_peek_book_manager (GmApp *self)
{
    return self->priv->book_manager;
}

GtkWindow *
gm_app_peek_first_window (GmApp *self)
{
    GList *l;

    for (l = gtk_application_get_windows (GTK_APPLICATION (self));
         l;
         l = g_list_next (l)) {
        /* if (GM_IS_WINDOW (l->data)) { */
        /*     return GTK_WINDOW (l->data); */
        /* } */
    }

    /* Create a new window */
    gm_app_new_window (self);

    /* And look for the newly created window again */
    return gm_app_peek_first_window (self);
}

GtkWindow *
gm_app_peek_assistant (GmApp *self)
{
    GList *l;

    for (l = gtk_application_get_windows (GTK_APPLICATION (self));
         l;
         l = g_list_next (l)) {
        /* if (GM_IS_ASSISTANT (l->data)) { */
        /*     return GTK_WINDOW (l->data); */
        /* } */
    }

    return NULL;
}

/******************************************************************************/
/* Application action activators */

void
gm_app_new_window (GmApp *self)
{
    g_action_group_activate_action (G_ACTION_GROUP (self), "new-window", NULL);
}

void
gm_app_quit (GmApp *self)
{
    g_action_group_activate_action (G_ACTION_GROUP (self), "quit", NULL);
}

void
gm_app_search (GmApp *self,
               const gchar *keyword)
{
    g_action_group_activate_action (G_ACTION_GROUP (self), "search", g_variant_new_string (keyword));
}

void
gm_app_search_assistant (GmApp *self,
                         const gchar *keyword)
{
    g_action_group_activate_action (G_ACTION_GROUP (self), "search-assistant", g_variant_new_string (keyword));
}

void
gm_app_raise (GmApp *self)
{
    g_action_group_activate_action (G_ACTION_GROUP (self), "raise", NULL);
}

/******************************************************************************/
/* Application actions setup */

static void
new_window_cb (GSimpleAction *action,
               GVariant      *parameter,
               gpointer       user_data)
{
    GmApp *self = GM_APP (user_data);
    GtkWidget *window;

    window = gm_window_new (self);
    gtk_application_add_window (GTK_APPLICATION (self), GTK_WINDOW (window));
    gtk_widget_show_all (window);
}

static void
preferences_cb (GSimpleAction *action,
                GVariant      *parameter,
                gpointer       user_data)
{
    /* gm_preferences_show_dialog (); */
}

static void
about_cb (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
    const gchar  *authors[] = {
        "Tubo <yangyingchao@gnome.org>",
        NULL
    };
    const gchar **documenters = NULL;
    const gchar  *translator_credits = _("translator_credits");

    /* i18n: Please don't translate "Gmget" (it's marked as translatable
     * for transliteration only) */
    gtk_show_about_dialog (NULL,
                           "name", _("Gmget"),
                           "version", "1.1",
                           "comments", _("GTK front end for mget"),
                           "authors", authors,
                           "documenters", documenters,
                           "translator-credits",
                           (strcmp (translator_credits, "translator_credits") != 0 ?
                            translator_credits :
                            NULL),
                           "website", "https://github.com/yangyingchao/mget",
                           "website-label", _("Gmget Website"),
                           "logo-icon-name", "gmget",
                           NULL);
}

static void
quit_cb (GSimpleAction *action,
         GVariant      *parameter,
         gpointer       user_data)
{
    GmApp *self = GM_APP (user_data);
    GList *l;

    /* Remove all windows registered in the application */
    while ((l = gtk_application_get_windows (GTK_APPLICATION (self)))) {
        gtk_application_remove_window (GTK_APPLICATION (self),
                                       GTK_WINDOW (l->data));
    }
}

static void
search_cb (GSimpleAction *action,
           GVariant      *parameter,
           gpointer       user_data)
{
    GmApp *self = GM_APP (user_data);
    GtkWindow *window;
    const gchar *str;

    window = gm_app_peek_first_window (self);
    str = g_variant_get_string (parameter, NULL);
    if (str[0] == '\0') {
        g_warning ("Cannot search in application window: "
                   "No keyword given");
        return;
    }

    /* gm_window_search (GM_WINDOW (window), str); */
    gtk_window_present (window);
}

static void
search_assistant_cb (GSimpleAction *action,
                     GVariant      *parameter,
                     gpointer       user_data)
{
    /* GmApp *self = GM_APP (user_data); */
    /* GtkWindow *assistant; */
    /* const gchar *str; */

    /* str = g_variant_get_string (parameter, NULL); */
    /* if (str[0] == '\0') { */
    /*     g_warning ("Cannot look for keyword in Search Assistant: " */
    /*                "No keyword given"); */
    /*     return; */
    /* } */

    /* /\* Look for an already registered assistant *\/ */
    /* assistant = gm_app_peek_assistant (self); */
    /* if (!assistant) { */
    /*     assistant = GTK_WINDOW (gm_assistant_new (self)); */
    /*     gtk_application_add_window (GTK_APPLICATION (self), assistant); */
    /* } */

    /* gm_assistant_search (GM_ASSISTANT (assistant), str); */
}

static void
raise_cb (GSimpleAction *action,
          GVariant      *parameter,
          gpointer       user_data)
{
    GmApp *self = GM_APP (user_data);
    GtkWindow *window;

    /* Look for the first application window available and show it */
    window = gm_app_peek_first_window (self);
    gtk_window_present (window);
}

static GActionEntry app_entries[] = {
    /* general  actions */
    { "new-window",       new_window_cb,       NULL, NULL, NULL },
    { "preferences",      preferences_cb,      NULL, NULL, NULL },
    { "about",            about_cb,            NULL, NULL, NULL },
    { "quit",             quit_cb,             NULL, NULL, NULL },
    /* additional commandline-specific actions */
    { "search",           search_cb,           "s",  NULL, NULL },
    { "search-assistant", search_assistant_cb, "s",  NULL, NULL },
    { "raise",            raise_cb,            NULL, NULL, NULL },
};

static void
setup_actions (GmApp *self)
{
    g_action_map_add_action_entries (G_ACTION_MAP (self),
                                     app_entries, G_N_ELEMENTS (app_entries),
                                     self);
}

/******************************************************************************/

static void
setup_accelerators (GmApp *self)
{
    gtk_application_add_accelerator (GTK_APPLICATION (self), "<Primary>0",     "win.zoom-default", NULL);
    gtk_application_add_accelerator (GTK_APPLICATION (self), "<Primary>minus", "win.zoom-out",     NULL);
    gtk_application_add_accelerator (GTK_APPLICATION (self), "<Primary>plus",  "win.zoom-in",      NULL);
    gtk_application_add_accelerator (GTK_APPLICATION (self), "<Primary>k",     "win.focus-search", NULL);
    gtk_application_add_accelerator (GTK_APPLICATION (self), "<Primary>f",     "win.find",         NULL);
    gtk_application_add_accelerator (GTK_APPLICATION (self), "<Primary>c",     "win.copy",         NULL);
    gtk_application_add_accelerator (GTK_APPLICATION (self), "<Primary>p",     "win.print",        NULL);
    gtk_application_add_accelerator (GTK_APPLICATION (self), "<Primary>t",     "win.new-tab",      NULL);
    gtk_application_add_accelerator (GTK_APPLICATION (self), "<Primary>w",     "win.close",        NULL);
    gtk_application_add_accelerator (GTK_APPLICATION (self), "F10",            "win.gear-menu",    NULL);
    gtk_application_add_accelerator (GTK_APPLICATION (self), "<Alt>Right",     "win.go-forward",   NULL);
    gtk_application_add_accelerator (GTK_APPLICATION (self), "<Alt>Left",      "win.go-back",      NULL);
}

/******************************************************************************/

static void
setup_menu (GmApp *self)
{
    GMenuModel *model;
    GError *error = NULL;
    model = G_MENU_MODEL (gtk_builder_get_object (
                              self->priv->menu_builder, "app-menu"));
    gtk_application_set_app_menu (GTK_APPLICATION (self), model);
}

static void
startup (GApplication *application)
{
    GmApp *self = GM_APP (application);

    /* Chain up parent's startup */
    G_APPLICATION_CLASS (gm_app_parent_class)->startup (application);

    GError        *error = NULL;
    self->priv->menu_builder = gtk_builder_new();
    if (!gtk_builder_add_from_file (self->priv->menu_builder,
                                    "res/menu.ui", &error))
    {
        g_error ("Cannot add resource to builder: %s",
                 error ? error->message : "unknown error");
        g_clear_error (&error);
    }

    /* Setup actions */
    setup_actions (self);

    /* Setup menu */
    setup_menu (self);

    /* Setup accelerators */
    setup_accelerators (self);

    /* Load the book manager */
    g_assert (self->priv->book_manager == NULL);
    self->priv->book_manager = NULL;//gm_book_manager_new ();
    /* gm_book_manager_populate (self->priv->book_manager); */
}

/******************************************************************************/

GmApp *
gm_app_new (void)
{
    GmApp *application;

    /* i18n: Please don't translate "Gmget" (it's marked as translatable
     * for transliteration only) */
    g_set_application_name (_("Gmget"));
    /* gtk_window_set_default_icon_name ("gmget"); */
    gtk_window_set_default_icon_from_file ("/mnt/data/Work/mget/src/ui/gtk/res/gmget.png",NULL);


    application = g_object_new (GM_TYPE_APP,
                                "application-id",   "org.gnome.Gmget",
                                "flags",            G_APPLICATION_FLAGS_NONE,
                                "register-session", TRUE,
                                NULL);

    return application;
}

static void
gm_app_init (GmApp *self)
{
    self->priv = G_TYPE_INSTANCE_GET_PRIVATE (self, GM_TYPE_APP, GmAppPrivate);
}

static void
dispose (GObject *object)
{
    GmApp *self = GM_APP (object);

    g_clear_object (&self->priv->book_manager);

    G_OBJECT_CLASS (gm_app_parent_class)->dispose (object);
}

static void
gm_app_class_init (GmAppClass *klass)
{
    GObjectClass *object_class = G_OBJECT_CLASS (klass);
    GApplicationClass *application_class = G_APPLICATION_CLASS (klass);

	g_type_class_add_private (klass, sizeof (GmAppPrivate));

    application_class->startup = startup;

    object_class->dispose = dispose;
}
