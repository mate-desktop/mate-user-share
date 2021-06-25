/* -*- Mode: C; indent-tabs-mode: t; c-basic-offset: 4; tab-width: 4 -*- */

/*
 *  Copyright (C) 2004-2008 Red Hat, Inc.
 * Copyright (C) 2012-2021 MATE Developers
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

#include <glib.h>
#include <glib/gi18n.h>
#include <X11/Xlib.h>

#include <gio/gio.h>

#include <stdarg.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>

#ifdef HAVE_SELINUX
#include <selinux/selinux.h>
#endif

#include "user_share-common.h"
#include "user_share-private.h"
#include "http.h"

/* From avahi-common/domain.h */
#define AVAHI_LABEL_MAX 64

#define GSETTINGS_SCHEMA "org.mate.FileSharing"

static pid_t httpd_pid = 0;

static int
get_port (void)
{
	int sock;
	struct sockaddr_in addr;
	int reuse;
	socklen_t len;

	sock = socket (PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		return -1;
	}

	memset (&addr, 0, sizeof (addr));
	addr.sin_port = 0;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_family = AF_INET;

	reuse = 1;
	setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof (reuse));
	if (bind (sock, (struct sockaddr *)&addr, sizeof (addr)) == -1) {
		close (sock);
		return -1;
	}

	len = sizeof (addr);
	if (getsockname (sock, (struct sockaddr *)&addr, &len) == -1) {
		close (sock);
		return -1;
	}

#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__APPLE__) || defined(__OpenBSD__)
	/* XXX This exposes a potential race condition, but without this,
	 * httpd will not start on the above listed platforms due to the fact
	 * that SO_REUSEADDR is also needed when Apache binds to the listening
	 * socket.  At this time, Apache does not support that socket option.
	 */
	close (sock);
#endif
	return ntohs (addr.sin_port);
}

static char *
truncate_name (const char *name)
{
	const char *end;

	end = g_utf8_find_prev_char (name, name + 64);
	g_assert (end != NULL);
	return g_strndup (name, end - name);
}

static char *
get_share_name (void)
{
	static char *name = NULL;
	const char *host_name;
	char *str;

	if (name == NULL) {
		host_name = g_get_host_name ();
		if (strcmp (host_name, "localhost") == 0) {
			/* Translators: The %s will get filled in with the user name
			   of the user, to form a genitive. If this is difficult to
			   translate correctly so that it will work correctly in your
			   language, you may use something equivalent to
			   "Public files of %s", or leave out the %s altogether.
			   In the latter case, please put "%.0s" somewhere in the string,
			   which will match the user name string passed by the C code,
			   but not put the user name in the final string. This is to
			   avoid the warning that msgfmt might otherwise generate. */
			name = g_strdup_printf (_("%s's public files"), g_get_user_name ());
		} else {
			/* Translators: This is similar to the string before, only it
			   has the hostname in it too. */
			name = g_strdup_printf (_("%s's public files on %s"),
						g_get_user_name (),
						host_name);
		}

	}
	/* And truncate */
	if (strlen (name) < AVAHI_LABEL_MAX)
		return name;

	str = truncate_name (name);
	g_free (name);
	name = str;

	return name;
}

static void
ensure_conf_dir (void)
{
	char *dirname;

	dirname = g_build_filename (g_get_user_config_dir (), "user-share", NULL);
	g_mkdir_with_parents (dirname, 0755);
	g_free (dirname);
}

static void
httpd_child_setup (gpointer user_data)
{
#ifdef HAVE_SELINUX
	char *mycon;
	/* If selinux is enabled, avoid transitioning to the httpd_t context,
	   as this normally means you can't read the users homedir. */
	if (is_selinux_enabled()) {
		if (getcon (&mycon) < 0) {
			abort ();
		}
		if (setexeccon (mycon) < 0)
			abort ();
		freecon (mycon);
	}
#endif
}

static const char *known_httpd_locations [] = {
	HTTPD_PROGRAM,
	"/usr/sbin/httpd",
	"/usr/sbin/httpd2",
	"/usr/sbin/apache2",
	NULL
};

static char*
get_httpd_program (void)
{
	int i;

	for (i = 0; known_httpd_locations[i]; i++) {
		if (known_httpd_locations[i][0] == '\0') {
			/* empty string as first element, happens when
			 * configured --with-httpd=auto */
			continue;
		}
		if (g_file_test (known_httpd_locations[i], G_FILE_TEST_IS_EXECUTABLE)
				&& ! g_file_test (known_httpd_locations[i], G_FILE_TEST_IS_DIR)) {
			return g_strdup (known_httpd_locations[i]);
		}
	}
	return NULL;
}

static const char *known_httpd_modules_locations [] = {
	HTTPD_MODULES_PATH,
	"/etc/httpd/modules",
	"/usr/lib/apache2/modules",
	NULL
};

static gchar*
get_httpd_modules_path (void)
{
	int i;

	for (i = 0; known_httpd_modules_locations[i]; i++) {
		if (known_httpd_modules_locations[i][0] == '\0') {
			/* empty string as first element, happens when
			 * configured --with-httpd=auto */
			continue;
		}
		if (g_file_test (known_httpd_modules_locations[i], G_FILE_TEST_IS_EXECUTABLE)
				&& g_file_test (known_httpd_modules_locations[i], G_FILE_TEST_IS_DIR)) {
			return g_strdup (known_httpd_modules_locations[i]);
		}
	}
	return NULL;
}

static GRegex *version_regex = NULL;

static char*
get_httpd_config (const char *httpd_program)
{
	gchar *standard_output;
	gchar *cmd_line;
	GMatchInfo *match_info;
	gchar *version_number = NULL;
	gchar *config;

	cmd_line = g_strdup_printf ("%s -v", httpd_program);
	if (! g_spawn_command_line_sync (cmd_line, &standard_output, NULL, NULL, NULL)) {
		g_free (cmd_line);
		return NULL;
	}
	g_free (cmd_line);

	if (version_regex == NULL) {
		version_regex = g_regex_new ("\\d\\.\\d", 0, 0, NULL);
	}

	if (g_regex_match (version_regex, standard_output, 0, &match_info)) {
		while (g_match_info_matches (match_info)) {
			version_number = g_match_info_fetch (match_info, 0);
			break;
		}
		g_match_info_free (match_info);
		g_free (standard_output);
	} else {
		/* Failed to parse httpd version number */
		g_warning ("Could not parse '%s' as a version for httpd", standard_output);
		g_free (standard_output);
		/* assume it is 2.2 */
		version_number = g_strdup ("2.2");
	}

	config = g_strdup_printf (HTTPD_CONFIG_TEMPLATE, version_number);
	g_free (version_number);

	return config;
}

static gboolean
spawn_httpd (int port, pid_t *pid_out)
{
	char *free1, *free2, *free3, *free4, *free5, *free6, *free7, *free8, *free9;
	gboolean res;
	char *argv[10];
	char *env[10];
	int i;
	gint status;
	char *pid_filename;
	char *pidfile;
	GError *error;
	gboolean got_pidfile;
	GSettings *settings;
	char *str;
	char *public_dir;

	public_dir = lookup_public_dir ();
	ensure_conf_dir ();

	i = 0;
	free1 = argv[i++] = get_httpd_program ();
	if (argv[0] == NULL) {
		fprintf (stderr, "error finding httpd server\n");
		return FALSE;
	}

	argv[i++] = "-f";
	free2 = argv[i++] = get_httpd_config (argv[0]);
	argv[i++] = "-C";
	free3 = argv[i++] = g_strdup_printf ("Listen %d", port);

	settings = g_settings_new (GSETTINGS_SCHEMA);
	str = g_settings_get_string (settings,
				       FILE_SHARING_REQUIRE_PASSWORD);

	if (str && strcmp (str, "never") == 0) {
		/* Do nothing */
	} else if (str && strcmp (str, "on_write") == 0){
		argv[i++] = "-D";
		argv[i++] = "RequirePasswordOnWrite";
	} else {
		/* always, or safe fallback */
		argv[i++] = "-D";
		argv[i++] = "RequirePasswordAlways";
	}
	g_free (str);
	g_object_unref (settings);

	argv[i] = NULL;

	i = 0;
	free4 = env[i++] = g_strdup_printf ("HOME=%s", g_get_home_dir());
	free5 = env[i++] = g_strdup_printf ("XDG_PUBLICSHARE_DIR=%s", public_dir);
	free6 = env[i++] = g_strdup_printf ("XDG_CONFIG_HOME=%s", g_get_user_config_dir ());
	free7 = env[i++] = g_strdup_printf ("GUS_SHARE_NAME=%s", get_share_name ());
	free8 = env[i++] = g_strdup_printf ("GUS_LOGIN_LABEL=%s", "Please log in as the user guest");
	free9 = env[i++] = g_strdup_printf ("HTTP_MODULES_PATH=%s",get_httpd_modules_path ());
	env[i++] = "LANG=C";
	env[i] = NULL;

	pid_filename = g_build_filename (g_get_user_config_dir (), "user-share", "pid", NULL);

	/* Remove pid file before spawning to avoid races with child and old pidfile */
	unlink (pid_filename);

	error = NULL;
	res = g_spawn_sync (g_get_home_dir(),
			    argv, env, 0,
			    httpd_child_setup, NULL,
			    NULL, NULL,
			    &status,
			    &error);
	g_free (free1);
	g_free (free2);
	g_free (free3);
	g_free (free4);
	g_free (free5);
	g_free (free6);
	g_free (free7);
	g_free (free8);
	g_free (free9);
	g_free (public_dir);

	if (!res) {
		fprintf (stderr, "error spawning httpd: %s\n",
			 error->message);
		g_error_free (error);
		g_free (pid_filename);
		return FALSE;
	}

	if (status != 0) {
		g_free (pid_filename);
		return FALSE;
	}

	got_pidfile = FALSE;
	error = NULL;
	for (i = 0; i < 5; i++) {
		if (error != NULL)
			g_error_free (error);
		error = NULL;
		if (g_file_get_contents (pid_filename, &pidfile, NULL, &error)) {
			got_pidfile = TRUE;
			*pid_out = atoi (pidfile);
			g_free (pidfile);
			break;
		}
		sleep (1);
	}

	g_free (pid_filename);

	if (!got_pidfile) {
		fprintf (stderr, "error opening httpd pidfile: %s\n", error->message);
		g_error_free (error);
		return FALSE;
	}
	return TRUE;
}

static void
kill_httpd (void)
{
	if (httpd_pid != 0) {
		kill (httpd_pid, SIGTERM);

		/* Allow child time to die, we can't waitpid, because its
		   not a direct child */
		sleep (1);
	}
	httpd_pid = 0;
}

void
http_up (void)
{
	guint port;

	port = get_port ();
	if (!spawn_httpd (port, &httpd_pid)) {
		fprintf (stderr, "spawning httpd failed\n");
	}
}

void
http_down (void)
{
	kill_httpd ();
}

gboolean
http_init (void)
{
	return TRUE;
}

pid_t
http_get_pid (void)
{
	return httpd_pid;
}
