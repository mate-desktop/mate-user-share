/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */

/*
 *  Copyright (C) 2004-2008 Red Hat, Inc.
 *
 *  Caja is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation; either version 2 of the
 *  License, or (at your option) any later version.
 *
 *  Caja is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 *  Authors: Bastien Nocera <hadess@hadess.net>
 *
 */

#include "config.h"

#include <glib.h>
#include <glib/gi18n.h>
#include <gio/gio.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>
#include <libmatenotify/notify.h>
#include <dbus/dbus-glib.h>
#include <dbus/dbus-glib-lowlevel.h>
#include <canberra-gtk.h>

#include <string.h>

#include "marshal.h"
#include "obexpush.h"
#include "user_share.h"
#include "user_share-private.h"

#define DBUS_TYPE_G_STRING_VARIANT_HASHTABLE (dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_STRING))

#define GSETTINGS_SCHEMA "org.mate.FileSharing"

static DBusGConnection *connection = NULL;
static DBusGProxy *manager_proxy = NULL;
static DBusGProxy *server_proxy = NULL;
static AcceptSetting accept_setting = -1;
static gboolean show_notifications = FALSE;

static GtkStatusIcon *statusicon = NULL;
static guint num_notifications = 0;

static void
hide_statusicon (void)
{
	num_notifications--;
	if (num_notifications == 0)
		gtk_status_icon_set_visible (statusicon, FALSE);
}

static void
on_close_notification (NotifyNotification *notification)
{
	hide_statusicon ();
	g_object_unref (notification);
}

static void
notification_launch_action_on_file_cb (NotifyNotification *notification,
				       const char *action,
				       const char *file_uri)
{
	GdkScreen *screen;
	GAppLaunchContext *ctx;
	GTimeVal val;

	g_assert (action != NULL);

	g_get_current_time (&val);

	ctx = G_APP_LAUNCH_CONTEXT (gdk_app_launch_context_new ());
	screen = gdk_screen_get_default ();
	gdk_app_launch_context_set_screen (GDK_APP_LAUNCH_CONTEXT (ctx), screen);
	gdk_app_launch_context_set_timestamp (GDK_APP_LAUNCH_CONTEXT (ctx), val.tv_sec);

	/* We launch the file viewer for the file */
	if (g_str_equal (action, "display") != FALSE) {
		if (g_app_info_launch_default_for_uri (file_uri, ctx, NULL) == FALSE) {
			g_warning ("Failed to launch the file viewer\n");
		}
	}

	/* we open the Downloads folder */
	if (g_str_equal (action, "reveal") != FALSE) {
		GFile *file;
		GFile *parent;
		gchar *parent_uri;

		file = g_file_new_for_uri (file_uri);
		parent = g_file_get_parent (file);
		parent_uri = g_file_get_uri (parent);
		g_object_unref (file);
		g_object_unref (parent);

		if (!g_app_info_launch_default_for_uri (parent_uri, ctx, NULL)) {
			g_warning ("Failed to launch the file manager\n");
		}

		g_free (parent_uri);
	}

	notify_notification_close (notification, NULL);
	/* No need to call hide_statusicon(), closing the notification
	 * will call the close callback */
}

static void
show_notification (const char *filename)
{
	char *file_uri, *notification_text, *display, *mime_type;
	NotifyNotification *notification;
	ca_context *ctx;
	GAppInfo *app;

	file_uri = g_filename_to_uri (filename, NULL, NULL);
	if (file_uri == NULL) {
		g_warning ("Could not make a filename from '%s'", filename);
		return;
	}

	display = g_filename_display_basename (filename);
	/* Translators: %s is the name of the filename received */
	notification_text = g_strdup_printf(_("You received \"%s\" via Bluetooth"), display);
	g_free (display);
	notification = notify_notification_new (_("You received a file"),
								 notification_text,
								 "dialog-information",
                                 NULL);

	notify_notification_set_timeout (notification, NOTIFY_EXPIRES_DEFAULT);

	mime_type = g_content_type_guess (filename, NULL, 0, NULL);
	app = g_app_info_get_default_for_type (mime_type, FALSE);
	if (app != NULL) {
		g_object_unref (app);
		notify_notification_add_action (notification, "display", _("Open File"),
						(NotifyActionCallback) notification_launch_action_on_file_cb,
						g_strdup (file_uri), (GFreeFunc) g_free);
	}
	notify_notification_add_action (notification, "reveal", _("Reveal File"),
					(NotifyActionCallback) notification_launch_action_on_file_cb,
					g_strdup (file_uri), (GFreeFunc) g_free);

	g_free (file_uri);
	
	g_signal_connect (G_OBJECT (notification), "closed", G_CALLBACK (on_close_notification), notification);

	if (!notify_notification_show (notification, NULL)) {
		g_warning ("failed to send notification\n");
	}
	g_free (notification_text);

	/* Now we do the audio notification */
	ctx = ca_gtk_context_get ();
	ca_context_play (ctx, 0,
			 CA_PROP_EVENT_ID, "complete-download",
			 CA_PROP_EVENT_DESCRIPTION, _("File reception complete"),
			 NULL);
}

static void
show_icon (void)
{
	if (statusicon == NULL) {
		statusicon = gtk_status_icon_new_from_icon_name ("mate-obex-server");
	} else {
		gtk_status_icon_set_visible (statusicon, TRUE);
	}
	num_notifications++;
}

static gboolean
device_is_authorised (const char *bdaddr)
{
	DBusGConnection *connection;
	DBusGProxy *manager;
	GError *error = NULL;
	GPtrArray *adapters;
	gboolean retval = FALSE;
	guint i;

	connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);
	if (connection == NULL)
		return FALSE;

	manager = dbus_g_proxy_new_for_name (connection, "org.bluez",
					     "/", "org.bluez.Manager");
	if (manager == NULL) {
		dbus_g_connection_unref (connection);
		return FALSE;
	}

	if (dbus_g_proxy_call (manager, "ListAdapters", &error, G_TYPE_INVALID, dbus_g_type_get_collection ("GPtrArray", DBUS_TYPE_G_OBJECT_PATH), &adapters, G_TYPE_INVALID) == FALSE) {
		g_object_unref (manager);
		dbus_g_connection_unref (connection);
		return FALSE;
	}

	for (i = 0; i < adapters->len; i++) {
		DBusGProxy *adapter, *device;
		char *device_path;
		GHashTable *props;

		g_debug ("checking adapter %s", (gchar *) g_ptr_array_index (adapters, i));

		adapter = dbus_g_proxy_new_for_name (connection, "org.bluez",
						    g_ptr_array_index (adapters, i), "org.bluez.Adapter");

		if (dbus_g_proxy_call (adapter, "FindDevice", NULL,
				       G_TYPE_STRING, bdaddr, G_TYPE_INVALID,
				       DBUS_TYPE_G_OBJECT_PATH, &device_path, G_TYPE_INVALID) == FALSE)
		{
			g_object_unref (adapter);
			continue;
		}

		device = dbus_g_proxy_new_for_name (connection, "org.bluez", device_path, "org.bluez.Device");

		if (dbus_g_proxy_call (device, "GetProperties", NULL,
				       G_TYPE_INVALID, dbus_g_type_get_map ("GHashTable", G_TYPE_STRING, G_TYPE_VALUE),
				       &props, G_TYPE_INVALID) != FALSE)
		{
			GValue *value;
			gboolean bonded;

			value = g_hash_table_lookup (props, "Paired");
			bonded = g_value_get_boolean (value);
			g_message ("%s is %s", bdaddr, bonded ? "bonded" : "not bonded");

			if (bonded) {
				g_hash_table_destroy (props);
				g_object_unref (device);
				g_object_unref (adapter);
				retval = TRUE;
				break;
			}
		}
		g_object_unref(adapter);
	}

	g_ptr_array_free (adapters, TRUE);

	g_object_unref(manager);
	dbus_g_connection_unref(connection);

	return retval;
}

static void
transfer_started_cb (DBusGProxy *session,
		     const char *filename,
		     const char *local_path,
		     guint64 size,
		     gpointer user_data)
{
	GHashTable *dict;
	DBusGProxy *server = (DBusGProxy *) user_data;
	GError *error = NULL;
	gboolean authorise;

	g_message ("transfer started on %s", local_path);
	g_object_set_data_full (G_OBJECT (session), "filename", g_strdup (local_path), (GDestroyNotify) g_free);

	show_icon ();

	/* First transfer of the session */
	if (g_object_get_data (G_OBJECT (session), "bdaddr") == NULL) {
		const char *bdaddr;

		if (dbus_g_proxy_call (server, "GetServerSessionInfo", &error,
				       DBUS_TYPE_G_OBJECT_PATH, dbus_g_proxy_get_path (session), G_TYPE_INVALID,
				       DBUS_TYPE_G_STRING_VARIANT_HASHTABLE, &dict, G_TYPE_INVALID) == FALSE) {
			g_printerr ("Getting Server session info failed: %s\n",
				    error->message);
			g_error_free (error);
			return;
		}

		bdaddr = g_hash_table_lookup (dict, "BluetoothAddress");
		g_message ("transfer started for device %s", bdaddr);

		g_object_set_data_full (G_OBJECT (session), "bdaddr", g_strdup (bdaddr), (GDestroyNotify) g_free);
		/* Initial accept method is undefined, we do that lower down */
		g_object_set_data (G_OBJECT (session), "accept-method", GINT_TO_POINTER (-1));
		g_hash_table_destroy (dict);
	}

	/* Accept method changed */
	if (GPOINTER_TO_INT (g_object_get_data (G_OBJECT (session), "accept-method")) != accept_setting) {
		const char *bdaddr;

		bdaddr = g_object_get_data (G_OBJECT (session), "bdaddr");

		if (bdaddr == NULL) {
			g_warning ("Couldn't get the Bluetooth address for the device, rejecting the transfer");
			authorise = FALSE;
		} else if (accept_setting == ACCEPT_ALWAYS) {
			authorise = TRUE;
		} else if (accept_setting == ACCEPT_BONDED) {
			authorise = device_is_authorised (bdaddr);
		} else {
			//FIXME implement
			g_warning ("\"Ask\" authorisation method not implemented");
			authorise = FALSE;
		}
		g_object_set_data (G_OBJECT (session), "authorise", GINT_TO_POINTER (authorise));
	}

	g_message ("accept_setting: %d", accept_setting);

	authorise = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (session), "authorise"));
	if (authorise != FALSE) {
		if (dbus_g_proxy_call (session, "Accept", &error, G_TYPE_INVALID, G_TYPE_INVALID) == FALSE) {
			g_printerr ("Failed to accept file transfer: %s\n", error->message);
			g_error_free (error);
			return;
		}
		g_message ("authorised transfer");
	} else {
		if (dbus_g_proxy_call (session, "Reject", &error, G_TYPE_INVALID, G_TYPE_INVALID) == FALSE) {
			g_printerr ("Failed to reject file transfer: %s\n", error->message);
			g_error_free (error);
			return;
		}
		g_message ("rejected transfer");
		g_object_set_data (G_OBJECT (session), "filename", NULL);
	}
}

static void
transfer_completed_cb (DBusGProxy *session,
		       gpointer user_data)
{
	GSettings *settings;
	gboolean display_notify; 
	const char *filename;

	filename = (const char *) g_object_get_data (G_OBJECT (session), "filename");

	g_message ("file finish transfer: %s", filename);

	if (filename == NULL)
		return;
	
	settings = g_settings_new (GSETTINGS_SCHEMA);	
	display_notify = g_settings_get_boolean (settings, FILE_SHARING_BLUETOOTH_OBEXPUSH_NOTIFY);
	g_object_unref (settings);
	
	if (display_notify) {
		show_notification (filename);
	} else {
		hide_statusicon ();
	}
	g_object_set_data (G_OBJECT (session), "filename", NULL);
}

static void
cancelled_cb (DBusGProxy *session,
	      gpointer user_data)
{
	//FIXME implement properly, we never actually finished the transfer
	g_message ("transfered was cancelled by the sender");
	transfer_completed_cb (session, user_data);
	hide_statusicon ();
}

static void
error_occurred_cb (DBusGProxy *session,
		   const char *error_name,
		   const char *error_message,
		   gpointer user_data)
{
	//FIXME implement properly
	g_message ("transfer error occurred: %s", error_message);
	transfer_completed_cb (session, user_data);
}

static void
session_created_cb (DBusGProxy *server, const char *session_path, gpointer user_data)
{
	DBusGProxy *session;

	session = dbus_g_proxy_new_for_name (connection,
					     "org.openobex",
					     session_path,
					     "org.openobex.ServerSession");

	dbus_g_proxy_add_signal (session, "TransferStarted",
				 G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT64, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(session, "TransferStarted",
				    G_CALLBACK (transfer_started_cb), server, NULL);
	dbus_g_proxy_add_signal (session, "TransferCompleted",
				 G_TYPE_INVALID, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(session, "TransferCompleted",
				    G_CALLBACK (transfer_completed_cb), server, NULL);
	dbus_g_proxy_add_signal (session, "Cancelled",
				 G_TYPE_INVALID, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(session, "Cancelled",
				    G_CALLBACK (cancelled_cb), server, NULL);
	dbus_g_proxy_add_signal (session, "ErrorOccurred",
				 G_TYPE_INVALID, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal(session, "ErrorOccurred",
				    G_CALLBACK (error_occurred_cb), server, NULL);
}

void
obexpush_up (void)
{
	GError *err = NULL;
	char *download_dir, *server;

	server = NULL;
	if (manager_proxy == NULL) {
		manager_proxy = dbus_g_proxy_new_for_name (connection,
							   "org.openobex",
							   "/org/openobex",
							   "org.openobex.Manager");
		if (dbus_g_proxy_call (manager_proxy, "CreateBluetoothServer",
				       &err, G_TYPE_STRING, "00:00:00:00:00:00", G_TYPE_STRING, "opp", G_TYPE_BOOLEAN, FALSE, G_TYPE_INVALID,
				       DBUS_TYPE_G_OBJECT_PATH, &server, G_TYPE_INVALID) == FALSE) {
			g_printerr ("Creating Bluetooth ObexPush server failed: %s\n",
				    err->message);
			g_error_free (err);
			g_object_unref (manager_proxy);
			manager_proxy = NULL;
			return;
		}
	}

	download_dir = lookup_download_dir ();

	if (server_proxy == NULL) {
		server_proxy = dbus_g_proxy_new_for_name (connection,
							   "org.openobex",
							   server,
							   "org.openobex.Server");
		g_free (server);

		dbus_g_proxy_add_signal (server_proxy, "SessionCreated",
					 DBUS_TYPE_G_OBJECT_PATH, G_TYPE_INVALID);
		dbus_g_proxy_connect_signal(server_proxy, "SessionCreated",
					    G_CALLBACK (session_created_cb), NULL, NULL);
	}

	if (dbus_g_proxy_call (server_proxy, "Start", &err,
			   G_TYPE_STRING, download_dir, G_TYPE_BOOLEAN, TRUE, G_TYPE_BOOLEAN, FALSE, G_TYPE_INVALID,
			   G_TYPE_INVALID) == FALSE) {
		if (g_error_matches (err, DBUS_GERROR, DBUS_GERROR_REMOTE_EXCEPTION) != FALSE &&
		    dbus_g_error_has_name (err, "org.openobex.Error.Started") != FALSE) {
		    	g_error_free (err);
		    	g_message ("already started, ignoring error");
		    	g_free (download_dir);
		    	return;
		}
		g_printerr ("Starting Bluetooth ObexPush server failed: %s\n",
			    err->message);
		g_error_free (err);
		g_free (download_dir);
		g_object_unref (server_proxy);
		server_proxy = NULL;
		g_object_unref (manager_proxy);
		manager_proxy = NULL;
		return;
	}

	g_free (download_dir);
}

static void
obexpush_stop (gboolean stop_manager)
{
	GError *err = NULL;

	if (server_proxy == NULL)
		return;

	if (dbus_g_proxy_call (server_proxy, "Close", &err, G_TYPE_INVALID, G_TYPE_INVALID) == FALSE) {
		if (g_error_matches (err, DBUS_GERROR, DBUS_GERROR_REMOTE_EXCEPTION) == FALSE ||
		    dbus_g_error_has_name (err, "org.openobex.Error.NotStarted") == FALSE) {
			g_printerr ("Stopping Bluetooth ObexPush server failed: %s\n",
				    err->message);
			g_error_free (err);
			return;
		}
		g_error_free (err);
	}

	if (stop_manager != FALSE) {
		g_object_unref (server_proxy);
		server_proxy = NULL;
		g_object_unref (manager_proxy);
		manager_proxy = NULL;
	}

	//FIXME stop all the notifications
}

void
obexpush_down (void)
{
	obexpush_stop (TRUE);
}

void
obexpush_restart (void)
{
	obexpush_stop (FALSE);
	obexpush_up ();
}

gboolean
obexpush_init (void)
{
	GError *err = NULL;

	connection = dbus_g_bus_get (DBUS_BUS_SESSION, &err);
	if (connection == NULL) {
		g_printerr ("Connecting to session bus failed: %s\n",
			    err->message);
		g_error_free (err);
		return FALSE;
	}

	dbus_g_object_register_marshaller (marshal_VOID__STRING_STRING_UINT64,
					   G_TYPE_NONE,
					   G_TYPE_STRING, G_TYPE_STRING, G_TYPE_UINT64, G_TYPE_INVALID);

	if (!notify_init("mate-user-share")) {
		g_warning("Unable to initialize the notification system\n");    
        }
	
	dbus_connection_set_exit_on_disconnect (dbus_g_connection_get_connection (connection), FALSE);

	return TRUE;
}

void
obexpush_set_accept_files_policy (AcceptSetting setting)
{
	accept_setting = setting;
}

void
obexpush_set_notify (gboolean enabled)
{
	show_notifications = enabled;
}
