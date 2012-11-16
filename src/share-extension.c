/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 8; tab-width: 8 -*-
 *
 * Copyright (C) 2003 Novell, Inc.
 * Copyright (C) 2003-2004 Red Hat, Inc.
 * Copyright (C) 2005 William Jon McCann <mccann@jhu.edu>
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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include <stdlib.h>
#include <string.h>
#include <glib/gi18n-lib.h>
#include <gtk/gtk.h>
#include <bluetooth-client.h>
#include <libcaja-extension/caja-menu-provider.h>
#include <libcaja-extension/caja-location-widget-provider.h>

#include "caja-share-bar.h"
#include "user_share-common.h"

#define CAJA_TYPE_USER_SHARE  (caja_user_share_get_type ())
#define CAJA_USER_SHARE(o)    (G_TYPE_CHECK_INSTANCE_CAST ((o), CAJA_TYPE_USER_SHARE, CajaUserShare))
#define CAJA_IS_USER_SHARE(o) (G_TYPE_CHECK_INSTANCE_TYPE ((o), CAJA_TYPE_USER_SHARE))

typedef struct
{
        GObject              parent_slot;
} CajaUserShare;

typedef struct
{
        GObjectClass parent_slot;
} CajaUserShareClass;

static GType caja_user_share_get_type      (void);
static void  caja_user_share_register_type (GTypeModule *module);

static void
launch_process (char **argv, GtkWindow *parent)
{
        GError *error;
        GtkWidget *dialog;

        error = NULL;
        if (!g_spawn_async (NULL,
                            argv, NULL,
                            0,
                            NULL, NULL,
                            NULL,
                            &error)) {


                dialog = gtk_message_dialog_new (NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_WARNING,
						 GTK_BUTTONS_OK, _("Unable to launch the Personal File Sharing preferences"));

                gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", error->message);

                gtk_dialog_run (GTK_DIALOG (dialog));
                gtk_widget_destroy (dialog);

                g_error_free (error);
        }
}

static void
launch_prefs_on_window (GtkWindow *window)
{
        char *argv [2];

        argv [0] = g_build_filename (BINDIR, "mate-file-share-properties", NULL);
        argv [1] = NULL;

        launch_process (argv, window);

        g_free (argv [0]);
}

static void
bar_response_cb (CajaShareBar *bar,
                 gint response,
                 gpointer         data)
{
        if (response == CAJA_SHARE_BAR_RESPONSE_PREFERENCES) {
                launch_prefs_on_window (GTK_WINDOW (data));
        }
}

static void
downloads_bar_set_from_bluetooth_status (GtkWidget *bar)
{
	BluetoothClient *client;
	gboolean bt_powered;

	client = g_object_get_data (G_OBJECT (bar), "bluetooth-client");
	g_object_get (G_OBJECT (client),
		      "default-adapter-powered", &bt_powered,
		      NULL);
	gtk_widget_set_visible (bar, bt_powered);
}

static void
default_adapter_powered_cb (GObject    *gobject,
			    GParamSpec *pspec,
			    GtkWidget  *bar)
{
	downloads_bar_set_from_bluetooth_status (bar);
}

static GtkWidget *
caja_user_share_get_location_widget (CajaLocationWidgetProvider *iface,
                                         const char                     *uri,
                                         GtkWidget                      *window)
{
	GFile             *file;
	GtkWidget         *bar;
	guint              i;
	gboolean           enable = FALSE;
	GFile             *home;
	const GUserDirectory special_dirs[] = { G_USER_DIRECTORY_PUBLIC_SHARE, G_USER_DIRECTORY_DOWNLOAD };
	gboolean is_dir[] = { FALSE, FALSE };

	file = g_file_new_for_uri (uri);
	home = g_file_new_for_path (g_get_home_dir ());

	/* We don't show anything in $HOME */
	if (g_file_equal (home, file)) {
		g_object_unref (home);
		g_object_unref (file);
		return NULL;
	}

	g_object_unref (home);

	for (i = 0; i < G_N_ELEMENTS (special_dirs); i++) {
		GFile *dir;
		dir = lookup_dir_with_fallback (special_dirs[i]);
		if (g_file_equal (dir, file)) {
			enable = TRUE;
			is_dir[i] = TRUE;
		}
		g_object_unref (dir);
	}

	if (enable == FALSE)
		return NULL;

	if (is_dir[0] != FALSE && is_dir[1] != FALSE) {
		bar = caja_share_bar_new (_("May be used to share or receive files"));
	} else if (is_dir[0] != FALSE) {
		bar = caja_share_bar_new (_("May be shared over the network or Bluetooth"));
	} else {
		BluetoothClient *client;

		bar = caja_share_bar_new (_("May be used to receive files over Bluetooth"));

		gtk_widget_set_no_show_all (bar, TRUE);
		client = bluetooth_client_new ();
		g_object_set_data_full (G_OBJECT (bar), "bluetooth-client", client, g_object_unref);
		g_signal_connect (G_OBJECT (client), "notify::default-adapter-powered",
				  G_CALLBACK (default_adapter_powered_cb), bar);
		downloads_bar_set_from_bluetooth_status (bar);
	}

	g_signal_connect (bar, "response",
			  G_CALLBACK (bar_response_cb),
			  window);

	gtk_widget_show_all (bar);

	g_object_unref (file);

        return bar;
}

static void
caja_user_share_location_widget_provider_iface_init (CajaLocationWidgetProviderIface *iface)
{
        iface->get_widget = caja_user_share_get_location_widget;
}

static void
caja_user_share_instance_init (CajaUserShare *share)
{

}

static void
caja_user_share_class_init (CajaUserShareClass *klass)
{

}

static GType share_type = 0;

static GType
caja_user_share_get_type (void)
{
        return share_type;
}

static void
caja_user_share_register_type (GTypeModule *module)
{
        static const GTypeInfo info = {
                sizeof (CajaUserShareClass),
                (GBaseInitFunc) NULL,
                (GBaseFinalizeFunc) NULL,
                (GClassInitFunc) caja_user_share_class_init,
                NULL,
                NULL,
                sizeof (CajaUserShare),
                0,
                (GInstanceInitFunc) caja_user_share_instance_init,
        };

        static const GInterfaceInfo location_widget_provider_iface_info = {
                (GInterfaceInitFunc) caja_user_share_location_widget_provider_iface_init,
                NULL,
                NULL
        };

        share_type = g_type_module_register_type (module,
                                                 G_TYPE_OBJECT,
                                                 "CajaUserShare",
                                                 &info, 0);

        g_type_module_add_interface (module,
                                     share_type,
                                     CAJA_TYPE_LOCATION_WIDGET_PROVIDER,
                                     &location_widget_provider_iface_info);
}

void
caja_module_initialize (GTypeModule *module)
{
        caja_user_share_register_type (module);
        bindtextdomain (GETTEXT_PACKAGE, MATELOCALEDIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
}

void
caja_module_shutdown (void)
{
}

void
caja_module_list_types (const GType **types,
                            int          *num_types)
{
        static GType type_list [1];

        type_list[0] = CAJA_TYPE_USER_SHARE;

        *types = type_list;
        *num_types = 1;
}
