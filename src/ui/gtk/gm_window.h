/** gm_window.h --- main window class
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

#ifndef _GM_WINDOW_H_
#define _GM_WINDOW_H_

#include <gtk/gtk.h>
#include "../../lib/log.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GM_TYPE_WINDOW         (gm_window_get_type ())
#define GM_WINDOW(o)           (G_TYPE_CHECK_INSTANCE_CAST ((o), GM_TYPE_WINDOW, GmWindow))
#define GM_WINDOW_CLASS(k)     (G_TYPE_CHECK_CLASS_CAST ((k), GM_TYPE_WINDOW, GmWindowClass))
#define GM_IS_WINDOW(o)        (G_TYPE_CHECK_INSTANCE_TYPE ((o), GM_TYPE_WINDOW))
#define GM_IS_WINDOW_CLASS(k)  (G_TYPE_CHECK_CLASS_TYPE ((k), GM_TYPE_WINDOW))
#define GM_WINDOW_GET_CLASS(o) (G_TYPE_INSTANCE_GET_CLASS ((o), GM_TYPE_WINDOW, GmWindowClass))

typedef struct _GmWindow       GmWindow;
typedef struct _GmWindowClass  GmWindowClass;
typedef struct _GmWindowPriv   GmWindowPriv;
typedef struct _GmApp GmApp;

#define G_GET_WIDGET(B, T, N) (T*)(gtk_builder_get_object((B), N))

#define GD_PTR(T, v, p) T* v = (T*)(p)

typedef enum
{
    GM_UPDATE_PROGRESS_NEW_WINDOW = 1 << 0,
    GM_UPDATE_PROGRESS_NEW_TAB    = 1 << 1
} GmOpenLinkFlags;

struct _GmWindow {
    GtkApplicationWindow parent_instance;
    GmWindowPriv *priv;
};

typedef enum _task_status
{
    TS_CREATED = 0,
    TS_STARTED,
    TS_PAUSED,
    TS_FINISHED,
    TS_FAILED,
} task_status;


struct _GmWindowClass {
    GtkApplicationWindowClass parent_class;

    /* Signals */
    void (*update_progress) (void*       window,
                             const char* name,
                             const char* size,
                             double      percentage,
                             const char* speed,
                             const char* eta,
                             gpointer    user_data);

    void (*status_changed) (void* window,
                           const char* name,
                           task_status stat,
                           const char* msg,
                           gpointer   user_data);
};

GType      gm_window_get_type     (void) G_GNUC_CONST;
GtkWidget *gm_window_new          (GmApp       *application);
void       gm_window_search       (GmWindow    *window,
                                   const gchar *str);
void       _gm_window_display_uri (GmWindow    *window,
                                   const gchar *uri);

#ifdef __cplusplus
}
#endif

#endif /* _GM_WINDOW_H_ */
