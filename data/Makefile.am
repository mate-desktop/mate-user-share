SUBDIRS = icons

@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
desktop_in_files = mate-user-share-properties.desktop.in

autostartdir = $(sysconfdir)/xdg/autostart/
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
autostart_in_files = mate-user-share.desktop.in

appdir = $(datadir)/mate-user-share
app_DATA=dav_user_2.0.conf dav_user_2.2.conf dav_user_2.4.conf dav_groupfile file-share-properties.ui

gsettings_SCHEMAS = org.mate.FileSharing.gschema.xml
@GSETTINGS_RULES@

convertdir = $(datadir)/MateConf/gsettings
convert_DATA = mate-user-share.convert

EXTRA_DIST = $(gsettings_SCHEMAS) \
	$(app_DATA)		\
	$(desktop_in_files)	\
	mate-user-share.desktop.in.in \
	file-share-properties.ui

CLEANFILES = $(desktop_DATA) $(autostart_DATA)

-include $(top_srcdir)/git.mk
