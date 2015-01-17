/*
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

#ifndef __GM_APP_H__
#define __GM_APP_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS
#define GM_TYPE_APP         (gm_app_get_type ())
#define GM_APP(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GM_TYPE_APP, GmApp))
#define GM_APP_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GM_TYPE_APP, GmAppClass))
#define GM_IS_APP(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GM_TYPE_APP))
#define GM_IS_APP_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GM_TYPE_APP))
#define GM_APP_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GM_TYPE_APP, GmAppClass))
typedef struct _GmApp GmApp;
typedef struct _GmAppClass GmAppClass;
typedef struct _GmAppPrivate GmAppPrivate;

struct _GmApp {
    GtkApplication parent_instance;
    GmAppPrivate *priv;
};

struct _GmAppClass {
    GtkApplicationClass parent_class;
};

GType gm_app_get_type(void) G_GNUC_CONST;

GmApp *gm_app_new(void);

GtkWindow *gm_app_peek_first_window(GmApp * self);
GtkWindow *gm_app_peek_assistant(GmApp * self);

void gm_app_new_window(GmApp * self);
void gm_app_quit(GmApp * self);
void gm_app_search(GmApp * self, const gchar * keyword);
void gm_app_search_assistant(GmApp * self, const gchar * keyword);
void gm_app_raise(GmApp * self);

G_END_DECLS
#endif				/* __GM_APP_H__ */
