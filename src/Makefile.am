
MARSHALFILES = marshal.c marshal.h
BUILT_SOURCES = $(MARSHALFILES)

marshal.c: marshal.h
	 $(AM_V_GEN) $(GLIB_GENMARSHAL) --quiet --prefix=marshal $(srcdir)/marshal.list --body > marshal.c
marshal.h: marshal.list
	$(AM_V_GEN) $(GLIB_GENMARSHAL) --quiet --prefix=marshal $(srcdir)/marshal.list --header > marshal.h


bin_PROGRAMS= \
	mate-file-share-properties

libexec_PROGRAMS= \
	mate-user-share

noinst_LTLIBRARIES = libuser-share-common.la
libuser_share_common_la_SOURCES = user_share-common.c user_share-common.h

AM_CPPFLAGS=	\
	-DPREFIX=\""$(prefix)"\" 			\
	-DBINDIR=\""$(bindir)"\" 			\
	-DLIBEXECDIR=\""$(libexecdir)"\"		\
	-DUSER_SHARE_PROGRAM=\""$(libexecdir)/mate-user-share"\" \
	-DMATELOCALEDIR=\""$(datadir)/locale"\"	\
	-DDATADIR=\""$(datadir)/mate-user-share/"\"	\
	-DHTTPD_CONFIG_TEMPLATE=\""$(datadir)/mate-user-share/dav_user_%s.conf"\"	\
	-DHTTPD_PROGRAM=\""$(HTTPD)"\"	\
	-DHTTPD_MODULES_PATH=\""$(MODULES_PATH)"\"	\
	-I$(top_srcdir) 				\
	-I$(top_builddir) 				\
	$(EXTENSION_CFLAGS)				\
	$(USER_SHARE_CFLAGS)				\
	$(BLUETOOTH_CFLAGS)				\
	$(USER_SHARE_CONFIG_CFLAGS)			\
	$(X_CFLAGS)

mate_user_share_SOURCES =	\
	user_share.c		\
	user_share-private.h	\
	user_share-private.c	\
	http.c			\
	http.h			\
	$(MARSHALFILES)

mate_user_share_LDADD =	\
	libuser-share-common.la	\
	$(USER_SHARE_LIBS)	\
	$(SELINUX_LIBS)	\
	$(X_LIBS) $(X_PRE_LIBS) -lX11 $(X_EXTRA_LIBS)

if USE_BLUETOOTH
mate_user_share_SOURCES += \
	obexftp.c		\
	obexftp.h		\
	obexpush.c		\
	obexpush.h
mate_user_share_LDADD += $(BLUETOOTH_LIBS)
endif

mate_file_share_properties_SOURCES =	\
	file-share-properties.c		\
	user_share-private.h		\
	user_share-private.c

mate_file_share_properties_LDADD = \
	$(USER_SHARE_CONFIG_LIBS)

caja_extensiondir = $(CAJADIR)
caja_extension_LTLIBRARIES = libcaja-user-share.la

libcaja_user_share_la_SOURCES =	\
	caja-share-bar.c			\
	caja-share-bar.h			\
	share-extension.c			\
	$(NULL)

libcaja_user_share_la_LIBADD = libuser-share-common.la $(EXTENSION_LIBS)
libcaja_user_share_la_LDFLAGS = -avoid-version -module -no-undefined

EXTRA_DIST = marshal.list

CLEANFILES = $(BUILT_SOURCES)


-include $(top_srcdir)/git.mk
