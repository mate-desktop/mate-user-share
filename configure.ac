AC_INIT(mate-user-share, 1.28.0)

AC_CONFIG_MACRO_DIR([m4])

AM_INIT_AUTOMAKE([1.9 dist-xz no-dist-gzip check-news])
AM_CONFIG_HEADER(config.h)

# Enable silent build when available (Automake 1.11)
m4_ifdef([AM_SILENT_RULES],[AM_SILENT_RULES([yes])])

AM_SANITY_CHECK
AM_MAINTAINER_MODE

MATE_COMPILE_WARNINGS

YELP_HELP_INIT

AC_C_CONST
AC_PROG_CC
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_MAKE_SET
AM_DISABLE_STATIC
AM_PROG_LIBTOOL
AC_PATH_XTRA
PKG_PROG_PKG_CONFIG

AC_TYPE_PID_T
AC_TYPE_SIGNAL
AC_TYPE_SIZE_T
AC_TYPE_UID_T

AC_ARG_WITH([httpd],
	    [AS_HELP_STRING([--with-httpd],[Specify the binary used for the Apache httpd binary (default: httpd)])],
	    [HTTPD=$withval])
if test -z "$HTTPD" ; then
	AC_PATH_PROG([HTTPD], [httpd],,
		     [$PATH:/usr/sbin])
	if test -z "$HTTPD" ; then
		# As used in SUSE
		AC_PATH_PROG([HTTPD], [apache2],,
			     [$PATH:/usr/sbin])
	fi
	if test -z "$HTTPD" ; then
		# As used in Ubuntu
		AC_PATH_PROG([HTTPD], [httpd2],,
			     [$PATH:/usr/sbin])
	fi
fi

AC_SUBST(HTTPD)

BLUETOOTH_PKG="mate-bluetooth-1.0 >= 1.2.0"
AC_ARG_ENABLE(bluetooth, AS_HELP_STRING([--disable-bluetooth],[compile without bluetooth support]),,enable_bluetooth=no)
AS_IF([test "x$enable_bluetooth" = "xyes"],[
 PKG_CHECK_MODULES(BLUETOOTH, $BLUETOOTH_PKG)
 AC_DEFINE(HAVE_BLUETOOTH, 1, [Set to true if mate-bluetooth support is available])
],[
 BLUETOOTH_PKG=""
 AC_MSG_WARN([Bluetooth support is disabled.])
])
AM_CONDITIONAL(USE_BLUETOOTH, [test "$enable_bluetooth" = "yes"])

GLIB_REQUIRED_VERSION=2.50.0
GTK_REQUIRED_VERSION=3.22.0

dnl ==========================================================================

PKG_CHECK_MODULES(USER_SHARE, glib-2.0 >= $GLIB_REQUIRED_VERSION gio-2.0 gdk-x11-3.0 gtk+-3.0 >= $GTK_REQUIRED_VERSION dbus-glib-1 libnotify >= 0.7.0 libcanberra-gtk3 $BLUETOOTH_PKG)
AC_SUBST(USER_SHARE_CFLAGS)
AC_SUBST(USER_SHARE_LIBS)

PKG_CHECK_MODULES(USER_SHARE_CONFIG, glib-2.0 >= $GLIB_REQUIRED_VERSION gio-2.0 gtk+-3.0 >= $GTK_REQUIRED_VERSION)
AC_SUBST(USER_SHARE_CONFIG_CFLAGS)
AC_SUBST(USER_SHARE_CONFIG_LIBS)

AC_CHECK_LIB(socket, socket)

have_socket=no
AC_CHECK_FUNCS(socket, have_socket=yes)
if test $have_socket = no; then
  # socket is not in the default libraries.  See if it's in some other.
  for lib in bsd socket inet; do
    AC_CHECK_LIB($lib, socket, [
	  LIBS="$LIBS -l$lib"
	  have_socket=yes
	  AC_DEFINE(HAVE_SOCKET)
	  break])
  done
fi

AC_PATH_PROG([GLIB_GENMARSHAL],[glib-genmarshal])

dnl
dnl Enable gsettigns schema macros
dnl

GLIB_GSETTINGS

dnl where to install the mate-user-share binary
if test "x$prefix" = "xNONE"; then
  prefix=$ac_default_prefix
fi
if test "x$exec_prefix" = "xNONE"; then
   exec_prefix=$prefix
fi
installdir="`eval echo $libexecdir`"
AC_SUBST(installdir, $installdir)

dnl ==========================================================================

GETTEXT_PACKAGE=mate-user-share
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE],"$GETTEXT_PACKAGE", [The gettext package])
AM_GNU_GETTEXT_VERSION([0.19.8])
AM_GNU_GETTEXT([external])

dnl ==========================================================================
dnl Check for SELinux

AC_CHECK_LIB(selinux, is_selinux_enabled, [ SELINUX_LIBS=-lselinux
  AC_DEFINE(HAVE_SELINUX,[1],[Found Selinux]) ])
AC_SUBST(SELINUX_LIBS)

dnl ==========================================================================
dnl Check for the HTTPD modules path

AC_ARG_WITH([modules-path],
	    [AS_HELP_STRING([--with-modules-path],[Path where the httpd modules are located (default: /etc/httpd/modules/)])],
	    [with_modules_path=$withval])

MODULES_PATH=$with_modules_path
AC_SUBST(MODULES_PATH)

dnl===========================================================================
dnl Check for caja for the share bar

AC_ARG_WITH(cajadir,
	    AS_HELP_STRING([--with-cajadir=DIR],[Installation path for Caja extension @<:@auto@:>@]),
	    [ac_with_cajadir=$withval],
	    [ac_with_cajadir=""])

PKG_CHECK_MODULES(EXTENSION,
		   libcaja-extension >= 1.17.1 $BLUETOOTH_PKG)
if test "${ac_with_cajadir}" = ""; then
	ac_with_cajadir=`$PKG_CONFIG --variable=extensiondir libcaja-extension`
fi
AC_SUBST(CAJADIR)
AC_MSG_NOTICE([installing caja plugin in ${ac_with_cajadir}])
AC_SUBST([CAJADIR],[${ac_with_cajadir}])

AC_CONFIG_FILES([
Makefile
data/Makefile
data/icons/Makefile
data/icons/16x16/Makefile
data/icons/22x22/Makefile
data/icons/32x32/Makefile
data/icons/48x48/Makefile
data/libcaja-user-share.desktop.in
data/mate-user-share-webdav.desktop.in
data/mate-user-share-obexftp.desktop.in
data/mate-user-share-obexpush.desktop.in
help/Makefile
po/Makefile.in
src/Makefile
man/Makefile
])

AC_OUTPUT

if test "x$enable_bluetooth" = "xyes"; then
	AC_MSG_NOTICE([** Bluetooth support enabled (ObexPush and ObexFTP)])
else
	AC_MSG_NOTICE([   Bluetooth support disabled])
fi

dnl ---------------------------------------------------------------------------
dnl - Show summary
dnl ---------------------------------------------------------------------------

echo "
Do note Apache >= 2.2 and mod_dnssd 0.6 are required to use mate-user-share.

       mate-user-share $VERSION
       `echo mate-user-share $VERSION | sed "s/./=/g"`

       prefix:               ${prefix}
       compiler:             ${CC}
       cflags:               ${CFLAGS}
       warning flags:        ${WARN_CFLAGS}
       httpd location:       ${HTTPD}
       httpd modules path:   ${MODULES_PATH}
       caja extension path:  ${ac_with_cajadir}
"
