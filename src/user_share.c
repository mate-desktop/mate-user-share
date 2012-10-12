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
 *  Authors: Alexander Larsson <alexl@redhat.com>
 *
 */

#include "config.h"

#include <gdk/gdkx.h>
#include <gtk/gtk.h>
#include <glib/gi18n.h>
#include <bluetooth-client.h>
#include <X11/Xlib.h>

#include "user_share.h"
#include "user_share-private.h"
#include "user_share-common.h"
#include "http.h"
#include "obexftp.h"
#include "obexpush.h"

#include <dbus/dbus.h>
#include <dbus/dbus-glib.h>

#include <gio/gio.h>

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>

/* ConsoleKit */
#define CK_NAME			"org.freedesktop.ConsoleKit"
#define CK_INTERFACE		"org.freedesktop.ConsoleKit"
#define CK_MANAGER_PATH		"/org/freedesktop/ConsoleKit/Manager"
#define CK_MANAGER_INTERFACE	"org.freedesktop.ConsoleKit.Manager"
#define CK_SEAT_INTERFACE	"org.freedesktop.ConsoleKit.Seat"
#define CK_SESSION_INTERFACE	"org.freedesktop.ConsoleKit.Session"

static guint disabled_timeout_tag = 0;
static gboolean has_console = TRUE;

static BluetoothClient *client = NULL;
static gboolean bluetoothd_enabled = FALSE;

#define OBEX_ENABLED (bluetoothd_enabled && has_console)

#define GSETTINGS_SCHEMA "org.mate.FileSharing"
#define GSETTINGS_KEY_FILE_SHARING_ENABLED "enabled"
#define GSETTINGS_KEY_FILE_SHARING_BLUETOOTH_ENABLED "bluetooth-enabled"
#define GSETTINGS_KEY_FILE_SHARING_REQUIRE_PASSWORD "require-password"
#define GSETTINGS_KEY_FILE_SHARING_BLUETOOTH_ALLOW_WRITE "bluetooth-allow-write"
#define GSETTINGS_KEY_FILE_SHARING_BLUETOOTH_REQUIRE_PAIRING "bluetooth-require-pairing"
#define GSETTINGS_KEY_FILE_SHARING_BLUETOOTH_OBEXPUSH_ENABLED "bluetooth-obexpush-enabled"
#define GSETTINGS_KEY_FILE_SHARING_BLUETOOTH_OBEXPUSH_ACCEPT_FILES "bluetooth-accept-files"
#define GSETTINGS_KEY_FILE_SHARING_BLUETOOTH_OBEXPUSH_NOTIFY "bluetooth-notify"


static GSettings* settings;

static void
obex_services_start (void)
{
	GSettings *settings;

	if (bluetoothd_enabled == FALSE ||
	    has_console == FALSE)
	    	return;

	settings = g_settings_new(GSETTINGS_SCHEMA);
	
	if (g_settings_get_boolean (settings, FILE_SHARING_BLUETOOTH_OBEXPUSH_ENABLED) == TRUE) {
	    obexpush_up ();
	}
	if (g_settings_get_boolean (settings, FILE_SHARING_BLUETOOTH_ENABLED) == TRUE) {
	    obexftp_up ();
	}

	g_object_unref (client);
}

static void
obex_services_shutdown (void)
{
	obexpush_down ();
	obexftp_down ();
}

static void
sessionchanged_cb (void)
{
	DBusGConnection *dbus_connection;
	DBusGProxy *proxy_ck_manager;
	DBusGProxy *proxy_ck_session;

	gchar *ck_session_path;
	gboolean is_active = FALSE;
	GError *error = NULL;

	g_message ("Active session changed");

	dbus_connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, &error);
	if (!dbus_connection) {
	    g_warning ("Unable to connect to dbus");
	    dbus_g_connection_unref (dbus_connection);
	    return;
	}
    
	proxy_ck_manager = dbus_g_proxy_new_for_name (dbus_connection,
			   			      CK_NAME,
		    				      CK_MANAGER_PATH,
					              CK_MANAGER_INTERFACE);
	if (dbus_g_proxy_call (proxy_ck_manager, "GetCurrentSession",
			       &error, G_TYPE_INVALID,
			       DBUS_TYPE_G_OBJECT_PATH, &ck_session_path,
			       G_TYPE_INVALID) == FALSE) {
	    g_warning ("Couldn't request the name: %s", error->message);
	    dbus_g_connection_unref (dbus_connection);
	    g_object_unref (proxy_ck_manager);
	    g_error_free (error);
	    return;
	}
	
	proxy_ck_session = dbus_g_proxy_new_for_name (dbus_connection,
						      CK_NAME,
						      ck_session_path,
						      CK_SESSION_INTERFACE);

	if (dbus_g_proxy_call (proxy_ck_session, "IsActive",
			       &error, G_TYPE_INVALID,
			       G_TYPE_BOOLEAN, &is_active, 
			       G_TYPE_INVALID) == FALSE) {
	    
	    g_warning ("Couldn't request the name: %s", error->message);
	    dbus_g_connection_unref (dbus_connection);
	    g_object_unref (proxy_ck_manager);
	    g_object_unref (proxy_ck_session);
	    g_error_free (error);
	    return;
	}
	
	has_console = is_active;
	if (is_active) {
		obex_services_start ();
	} else {
		obex_services_shutdown (); 
	}

	dbus_g_connection_unref (dbus_connection);
	g_free (ck_session_path);
	g_object_unref (proxy_ck_manager);
	g_object_unref (proxy_ck_session);
	if (error != NULL) {
	    g_error_free (error);
	}
}

static void
consolekit_init (void)
{
	DBusGConnection *dbus_connection;
	DBusGProxy *proxy_ck_manager;
	DBusGProxy *proxy_ck_session;
	DBusGProxy *proxy_ck_seat;
	gchar *ck_session_path;
	gchar *ck_seat_path;
	GError *error = NULL;

	dbus_connection = dbus_g_bus_get (DBUS_BUS_SYSTEM, NULL);

	if (!dbus_connection) {
	    g_warning ("Unable to connect to dbus");
	    dbus_g_connection_unref (dbus_connection);
	    return;
	}
    
	proxy_ck_manager = dbus_g_proxy_new_for_name (dbus_connection,
						      CK_NAME,
						      CK_MANAGER_PATH,
						      CK_MANAGER_INTERFACE);
	if (dbus_g_proxy_call (proxy_ck_manager, "GetCurrentSession",
			       &error, G_TYPE_INVALID,
			       DBUS_TYPE_G_OBJECT_PATH, &ck_session_path,
			       G_TYPE_INVALID) == FALSE) {
	    
	    g_warning ("Couldn't request the name: %s", error->message);
	    g_object_unref (proxy_ck_manager);
	    return;
	}
	
	proxy_ck_session = dbus_g_proxy_new_for_name (dbus_connection,
						      CK_NAME,
						      ck_session_path,
						      CK_SESSION_INTERFACE);
	if (dbus_g_proxy_call (proxy_ck_session, "GetSeatId",
			       &error, G_TYPE_INVALID,
			       DBUS_TYPE_G_OBJECT_PATH, &ck_seat_path,
			       G_TYPE_INVALID) == FALSE) {
	    
	    g_warning ("Couldn't request the name: %s", error->message);
	    g_object_unref (proxy_ck_session); 
	    return;
	}
	
	proxy_ck_seat = dbus_g_proxy_new_for_name (dbus_connection,
						   CK_NAME,
						   ck_seat_path,
						   CK_SEAT_INTERFACE);
	dbus_g_proxy_add_signal (proxy_ck_seat, "ActiveSessionChanged",
				 G_TYPE_STRING, G_TYPE_INVALID);
	dbus_g_proxy_connect_signal (proxy_ck_seat, "ActiveSessionChanged",
				     G_CALLBACK (sessionchanged_cb), NULL, NULL);
	if (error != NULL) {
	    g_error_free (error);
	}
	g_object_unref (proxy_ck_manager);
	g_object_unref (proxy_ck_session);
	g_free (ck_seat_path);
	dbus_g_connection_unref (dbus_connection);
}

static void
default_adapter_changed (GObject    *gobject,
			 GParamSpec *pspec,
			 gpointer    user_data)
{
	char *adapter;
	gboolean adapter_powered;

	g_object_get (G_OBJECT (client),
		      "default-adapter", &adapter,
		      "default-adapter-powered", &adapter_powered,
		      NULL);
	if (adapter != NULL && *adapter != '\0') {
		bluetoothd_enabled = adapter_powered;
	} else {
		bluetoothd_enabled = FALSE;
	}

	/* Were we called as init, or as a callback */
	if (gobject != NULL) {
		if (bluetoothd_enabled != FALSE)
			obex_services_start ();
		else
			obex_services_shutdown ();
	}
}

static void
bluez_init (void)
{
	client = bluetooth_client_new ();
	default_adapter_changed (NULL, NULL, NULL);
	g_signal_connect (G_OBJECT (client), "notify::default-adapter",
			  G_CALLBACK (default_adapter_changed), NULL);
	g_signal_connect (G_OBJECT (client), "notify::default-adapter-powered",
			  G_CALLBACK (default_adapter_changed), NULL);
}

static void
migrate_old_configuration (void)
{
	const char *old_config_dir;
	const char *new_config_dir;

	old_config_dir = g_build_filename (g_get_home_dir (), ".mate2", "user-share", NULL);
	new_config_dir = g_build_filename (g_get_user_config_dir (), "user-share", NULL);
	if (g_file_test (old_config_dir, G_FILE_TEST_IS_DIR))
		g_rename (old_config_dir, new_config_dir);

}

static void
require_password_changed (GSettings *settings, gchar *key, gpointer data)
{
	/* Need to restart to get new password setting */
	if (http_get_pid () != 0) {
		http_down ();
		http_up ();
	}
}

/* File sharing was disabled for some time, exit now */
/* If we re-enable it in the ui, this will be restarted anyway */
static gboolean
disabled_timeout_callback (gpointer user_data)
{
	GSettings *settings = (GSettings *) user_data;
	http_down ();

	if (g_settings_get_boolean (settings, FILE_SHARING_BLUETOOTH_ENABLED) == FALSE &&
	    g_settings_get_boolean (settings, FILE_SHARING_BLUETOOTH_OBEXPUSH_ENABLED) == FALSE)
		_exit (0);
	return FALSE;
}

static void
file_sharing_enabled_changed (GSettings *settings, gchar *key, gpointer data)
{
	gboolean enabled;

	if (disabled_timeout_tag != 0) {
		g_source_remove (disabled_timeout_tag);
		disabled_timeout_tag = 0;
	}

	enabled = g_settings_get_boolean (settings,
					 FILE_SHARING_ENABLED);
	if (enabled) {
		if (http_get_pid () == 0) {
			http_up ();
		}
	} else {
		http_down ();
		disabled_timeout_tag = g_timeout_add_seconds (3,
							      (GSourceFunc)disabled_timeout_callback,
							      client);
	}
}

static void
file_sharing_bluetooth_allow_write_changed (GSettings *settings, gchar *key, gpointer data)
{
	if (g_settings_get_boolean (settings, FILE_SHARING_BLUETOOTH_ENABLED) != FALSE)
		obexftp_restart ();
}

static void
file_sharing_bluetooth_require_pairing_changed (GSettings *settings, gchar *key, gpointer data)
{
	if (g_settings_get_boolean (settings, FILE_SHARING_BLUETOOTH_ENABLED) != FALSE) {
		/* We need to fully reset the session,
		 * otherwise the new setting isn't taken into account */
		obexftp_down ();
		obexftp_up ();
	}
}

static void
file_sharing_bluetooth_enabled_changed (GSettings *settings, gchar *key, gpointer data)
{
	if (g_settings_get_boolean (settings,
				   FILE_SHARING_BLUETOOTH_ENABLED) == FALSE) {
		obexftp_down ();
		if (g_settings_get_boolean (settings, FILE_SHARING_ENABLED) == FALSE &&
		    g_settings_get_boolean (settings, FILE_SHARING_BLUETOOTH_OBEXPUSH_ENABLED) == FALSE) {
			_exit (0);
		}
	} else if (OBEX_ENABLED) {
		obexftp_up ();
	}
}

static void
file_sharing_bluetooth_obexpush_enabled_changed (GSettings *settings, gchar *key, gpointer data)
{
	if (g_settings_get_boolean (settings,
				   FILE_SHARING_BLUETOOTH_OBEXPUSH_ENABLED) == FALSE) {
		obexpush_down ();
		if (g_settings_get_boolean (settings, FILE_SHARING_ENABLED) == FALSE &&
		    g_settings_get_boolean (settings, FILE_SHARING_BLUETOOTH_ENABLED) == FALSE) {
			_exit (0);
		}
	} else if (OBEX_ENABLED) {
		obexpush_up ();
	}
}

static void
file_sharing_bluetooth_obexpush_accept_files_changed (GSettings *settings, gchar *key, gpointer data)
{
	AcceptSetting setting;
	char *str;

	str = g_settings_get_string (settings, FILE_SHARING_BLUETOOTH_OBEXPUSH_ACCEPT_FILES);
	setting = accept_setting_from_string (str);
	g_free (str);

	obexpush_set_accept_files_policy (setting);
}

static void
file_sharing_bluetooth_obexpush_notify_changed (GSettings *settings, gchar *key, gpointer data)
{
	obexpush_set_notify (g_settings_get_boolean (settings, FILE_SHARING_BLUETOOTH_OBEXPUSH_NOTIFY));
}

static RETSIGTYPE
cleanup_handler (int sig)
{
	http_down ();
	obexftp_down ();
	obexpush_down ();
	_exit (2);
}

static int
x_io_error_handler (Display *xdisplay)
{
	http_down ();
	obexftp_down ();
	_exit (2);
}

int
main (int argc, char **argv)
{
	GSettings *settings;
	Display *xdisplay;
	int x_fd;
	Window selection_owner;
	Atom xatom;

	bindtextdomain (GETTEXT_PACKAGE, MATELOCALEDIR);
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
	textdomain (GETTEXT_PACKAGE);

	gtk_init (&argc, &argv);

	if (g_strcmp0 (g_get_real_name (), "root") == 0) {
		g_warning ("mate-user-share cannot be started as root for security reasons.");
		return 1;
	}

	signal (SIGPIPE, SIG_IGN);
	signal (SIGINT, cleanup_handler);
	signal (SIGHUP, cleanup_handler);
	signal (SIGTERM, cleanup_handler);

	xdisplay = GDK_DISPLAY_XDISPLAY (gdk_display_get_default ());
	if (xdisplay == NULL) {
		g_warning ("Can't open display");
		return 1;
	}

	xatom = XInternAtom (xdisplay, "_MATE_USER_SHARE", FALSE);
	selection_owner = XGetSelectionOwner (xdisplay, xatom);

	if (selection_owner != None) {
		/* There is an owner already, quit */
		return 1;
	}

	selection_owner = XCreateSimpleWindow (xdisplay,
					       RootWindow (xdisplay, 0),
					       0, 0, 1, 1,
					       0, 0, 0);
	XSetSelectionOwner (xdisplay, xatom, selection_owner, CurrentTime);

	if (XGetSelectionOwner (xdisplay, xatom) != selection_owner) {
		/* Didn't get the selection */
		return 1;
	}

	migrate_old_configuration ();

	settings = g_settings_new (GSETTINGS_SCHEMA);
	if (g_settings_get_boolean (settings, FILE_SHARING_ENABLED) == FALSE &&
	    g_settings_get_boolean (settings, FILE_SHARING_BLUETOOTH_ENABLED) == FALSE &&
	    g_settings_get_boolean (settings, FILE_SHARING_BLUETOOTH_OBEXPUSH_ENABLED) == FALSE)
		return 1;

	x_fd = ConnectionNumber (xdisplay);
	XSetIOErrorHandler (x_io_error_handler);

	if (http_init () == FALSE)
		return 1;
	if (obexftp_init () == FALSE)
		return 1;
	if (obexpush_init () == FALSE)
		return 1;

    g_signal_connect (settings,
			     "changed::" GSETTINGS_KEY_FILE_SHARING_ENABLED,
			     G_CALLBACK (file_sharing_enabled_changed), NULL);

    g_signal_connect (settings,
			     "changed::" GSETTINGS_KEY_FILE_SHARING_REQUIRE_PASSWORD,
			     G_CALLBACK (require_password_changed), NULL);

    g_signal_connect (settings,
			     "changed::" GSETTINGS_KEY_FILE_SHARING_BLUETOOTH_ENABLED,
			     G_CALLBACK (file_sharing_bluetooth_enabled_changed), NULL);

    g_signal_connect (settings,
			     "changed::" GSETTINGS_KEY_FILE_SHARING_BLUETOOTH_ALLOW_WRITE,
			     G_CALLBACK (file_sharing_bluetooth_allow_write_changed), NULL);

    g_signal_connect (settings,
			     "changed::" GSETTINGS_KEY_FILE_SHARING_BLUETOOTH_REQUIRE_PAIRING,
			     G_CALLBACK (file_sharing_bluetooth_require_pairing_changed), NULL);

    g_signal_connect (settings,
			     "changed::" GSETTINGS_KEY_FILE_SHARING_BLUETOOTH_OBEXPUSH_ENABLED,
			     G_CALLBACK (file_sharing_bluetooth_obexpush_enabled_changed), NULL);

    g_signal_connect (settings,
			     "changed::" GSETTINGS_KEY_FILE_SHARING_BLUETOOTH_OBEXPUSH_ACCEPT_FILES,
			     G_CALLBACK (file_sharing_bluetooth_obexpush_accept_files_changed), NULL);

    g_signal_connect (settings,
			     "changed::" GSETTINGS_KEY_FILE_SHARING_BLUETOOTH_OBEXPUSH_NOTIFY,
			     G_CALLBACK (file_sharing_bluetooth_obexpush_notify_changed), NULL);


	bluez_init ();
	consolekit_init ();

	/* Initial setting */
	file_sharing_enabled_changed (settings, NULL, NULL);
	file_sharing_bluetooth_enabled_changed (settings, NULL, NULL);
	file_sharing_bluetooth_obexpush_accept_files_changed (settings, NULL, NULL);
	file_sharing_bluetooth_obexpush_notify_changed (settings, NULL, NULL);
	file_sharing_bluetooth_obexpush_enabled_changed (settings, NULL, NULL);

	gtk_main ();

	return 0;
}
