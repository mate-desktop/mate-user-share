SUBDIRS = icons

%.desktop: %.desktop.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
desktop_in_files = mate-user-share-properties.desktop.in

autostartdir = $(sysconfdir)/xdg/autostart/
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
autostart_in_files = mate-user-share-obexftp.desktop.in mate-user-share-obexpush.desktop.in mate-user-share-webdav.desktop.in

appdir = $(datadir)/mate-user-share
app_DATA=dav_user_2.2.conf dav_user_2.4.conf dav_groupfile file-share-properties.ui

gsettings_SCHEMAS = org.mate.FileSharing.gschema.xml
@GSETTINGS_RULES@

extensiondir = $(datadir)/caja/extensions
extension_in_files = libcaja-user-share.caja-extension.in
extension_DATA = $(extension_in_files:.caja-extension.in=.caja-extension)
%.caja-extension: %.caja-extension.in
	$(AM_V_GEN) $(MSGFMT) --desktop --template $< -d $(top_srcdir)/po -o $@

EXTRA_DIST = $(gsettings_SCHEMAS) \
	$(app_DATA)		\
	$(desktop_in_files)	\
	mate-user-share-obexftp.desktop.in.in \
	mate-user-share-obexpush.desktop.in.in \
	mate-user-share-webdav.desktop.in.in \
	file-share-properties.ui

CLEANFILES = $(extension_DATA) $(desktop_DATA) $(autostart_DATA)

-include $(top_srcdir)/git.mk
