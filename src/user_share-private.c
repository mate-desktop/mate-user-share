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
 *  Bastien Nocera <hadess@hadess.net>
 *
 */

#include "config.h"

#include <string.h>

#include "user_share-private.h"

static char *password_setting_strings[] = {
    "never",
    "on_write",
    "always"
};

static char *accept_file_strings[] = {
    "always",
    "bonded",
    "ask"
};

const char *
password_string_from_setting (PasswordSetting setting)
{

    if (setting >= 0 && setting <= PASSWORD_ALWAYS)
	return password_setting_strings[setting];

    /* Fallback on secure pref */
    return password_setting_strings[PASSWORD_ALWAYS];
}

PasswordSetting
password_setting_from_string (const char *str)
{
    if (str != NULL) {
	if (strcmp (str, "never") == 0) {
	    return PASSWORD_NEVER;
	}
	if (strcmp (str, "always") == 0) {
	    return PASSWORD_ALWAYS;
	}
	if (strcmp (str, "on_write") == 0) {
	    return PASSWORD_ON_WRITE;
	}
    }

    /* Fallback on secure pref */
    return PASSWORD_ALWAYS;
}

const char *
accept_string_from_setting (AcceptSetting setting)
{

    if (setting >= 0 && setting <= ACCEPT_ASK)
	return accept_file_strings[setting];

    /* Fallback on secure pref */
    return accept_file_strings[ACCEPT_BONDED];
}

AcceptSetting
accept_setting_from_string (const char *str)
{
    if (str != NULL) {
	if (strcmp (str, "always") == 0) {
	    return ACCEPT_ALWAYS;
	}
	if (strcmp (str, "bonded") == 0 ||
	    strcmp (str, "bonded_and_trusted") == 0) {
	    return ACCEPT_BONDED;
	}
	if (strcmp (str, "ask") == 0) {
	    return ACCEPT_ASK;
	}
    }

    /* Fallback on secure pref */
    return ACCEPT_BONDED;
}
