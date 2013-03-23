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

#ifndef _USER_SHARE_PRIVATE_H_
#define _USER_SHARE_PRIVATE_H_

#define FILE_SHARING_ENABLED "enabled"
#define FILE_SHARING_BLUETOOTH_ENABLED "bluetooth-enabled"
#define FILE_SHARING_BLUETOOTH_OBEXPUSH_ENABLED "bluetooth-obexpush-enabled"

#define FILE_SHARING_REQUIRE_PASSWORD "require-password"
#define FILE_SHARING_BLUETOOTH_ALLOW_WRITE "bluetooth-allow-write"
#define FILE_SHARING_BLUETOOTH_REQUIRE_PAIRING "bluetooth-require-pairing"
#define FILE_SHARING_BLUETOOTH_OBEXPUSH_ACCEPT_FILES "bluetooth-accept-files"
#define FILE_SHARING_BLUETOOTH_OBEXPUSH_NOTIFY "bluetooth-notify"

typedef enum {
    PASSWORD_NEVER,
    PASSWORD_ON_WRITE,
    PASSWORD_ALWAYS
} PasswordSetting;

typedef enum {
	ACCEPT_ALWAYS,
	ACCEPT_BONDED,
	ACCEPT_ASK
} AcceptSetting;

const char *password_string_from_setting (PasswordSetting setting);
PasswordSetting password_setting_from_string (const char *str);

const char *accept_string_from_setting (AcceptSetting setting);
AcceptSetting accept_setting_from_string (const char *str);

#endif /* _USER_SHARE_PRIVATE_H_ */
