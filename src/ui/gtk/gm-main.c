/*
 * Copyright (C) 2001-2003 CodeFactory AB
 * Copyright (C) 2001-2008 Imendio AB
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

#include <stdlib.h>
#include <locale.h>
#include <string.h>

#include <glib/gi18n.h>
#include <gtk/gtk.h>
#include "gm-app.h"

static gboolean option_new_window;
static gchar *option_search;
static gchar *option_search_assistant;
static gboolean option_quit;
static gboolean option_version;

static GOptionEntry options[] = {
    /* { "new-window", 'n', */
    /*   0, G_OPTION_ARG_NONE, &option_new_window, */
    /*   N_("Opens a new Devhelp window"), */
    /*   NULL */
    /* }, */
    /* { "search", 's', */
    /*   0, G_OPTION_ARG_STRING, &option_search, */
    /*   N_("Search for a keyword"), */
    /*   N_("KEYWORD") */
    /* }, */
    /* { "search-assistant", 'a', */
    /*   0, G_OPTION_ARG_STRING, &option_search_assistant, */
    /*   N_("Search and display any hit in the assistant window"), */
    /*   N_("KEYWORD") */
    /* }, */
    {"version", 'v',
     0, G_OPTION_ARG_NONE, &option_version,
     N_("Display the version and exit"),
     NULL},
    {"quit", 'q',
     0, G_OPTION_ARG_NONE, &option_quit,
     N_("Quit Gmget"),
     NULL},
    {NULL}
};

static void run_action(GmApp * application, gboolean is_remote)
{
    if (option_new_window) {
        if (is_remote)
            gm_app_new_window(application);
    } else if (option_quit) {
        gm_app_quit(application);
    } else if (option_search) {
        gm_app_search(application, option_search);
    } else if (option_search_assistant) {
        gm_app_search_assistant(application, option_search_assistant);
    } else {
        if (is_remote)
            gm_app_raise(application);
    }
}

static void activate_cb(GtkApplication * application)
{
    /* This is the primary instance */
    gm_app_new_window(GM_APP(application));

    /* Run the requested action from the command line */
    run_action(GM_APP(application), FALSE);
}

int main(int argc, char **argv)
{
    GmApp *application;
    GError *error = NULL;
    gint status;

    setlocale(LC_ALL, "");
    /* bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR); */
    /* bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8"); */
    /* textdomain (GETTEXT_PACKAGE); */
    gtk_init(&argc, &argv);

    // TODO: Remove this ifdef!
#if 0
    if (!gtk_init_with_args
        (&argc, &argv, NULL, options, GETTEXT_PACKAGE, &error)) {
        g_printerr("%s\n", error->message);
        return EXIT_FAILURE;
    }
#endif                          // End of #if 0

#if 0
    if (option_version) {
        g_print("%s\n", PACKAGE_STRING);
        return EXIT_SUCCESS;
    }
#endif                          // End of #if 0

/* Create new GmApp */
    application = gm_app_new();
    g_signal_connect(application, "activate", G_CALLBACK(activate_cb),
                     NULL);

    /* Set it as the default application */
    g_application_set_default(G_APPLICATION(application));

    /* Try to register the application... */
    if (!g_application_register(G_APPLICATION(application), NULL, &error)) {
        g_printerr("Couldn't register GMget instance: '%s'\n",
                   error ? error->message : "");
        g_object_unref(application);
        return EXIT_FAILURE;
    }

    /* Actions on a remote Devhelp already running? */
    if (g_application_get_is_remote(G_APPLICATION(application))) {
        /* Run the requested action from the command line */
        run_action(application, TRUE);
        g_object_unref(application);
        return EXIT_SUCCESS;
    }

    /* And run the GtkApplication */
    status = g_application_run(G_APPLICATION(application), argc, argv);

    g_object_unref(application);

    return status;
}
