SUBDIRS = icons

@INTLTOOL_SCHEMAS_RULE@
schemadir = $(MATECONF_SCHEMA_FILE_DIR)
schema_in_files = desktop_mate_file_sharing.schemas.in
schema_DATA = $(schema_in_files:.schemas.in=.schemas)
	
@INTLTOOL_DESKTOP_RULE@
desktopdir = $(datadir)/applications
desktop_DATA = $(desktop_in_files:.desktop.in=.desktop)
desktop_in_files = mate-user-share-properties.desktop.in

autostartdir = $(sysconfdir)/xdg/autostart/
autostart_DATA = $(autostart_in_files:.desktop.in=.desktop)
autostart_in_files = mate-user-share.desktop.in

appdir = $(datadir)/mate-user-share
app_DATA=dav_user_2.0.conf dav_user_2.2.conf dav_groupfile file-share-properties.ui

install-data-local:
if MATECONF_SCHEMAS_INSTALL
	-MATECONF_CONFIG_SOURCE=$(MATECONF_SCHEMA_CONFIG_SOURCE) \
	$(MATECONFTOOL) --makefile-install-rule $(schema_DATA)
endif

if MATECONF_SCHEMAS_INSTALL
uninstall-local:
	-MATECONF_CONFIG_SOURCE=$(MATECONF_SCHEMA_CONFIG_SOURCE) \
	$(MATECONFTOOL) --makefile-uninstall-rule $(schema_DATA)
endif

EXTRA_DIST = $(schema_in_files) \
	$(app_DATA)		\
	$(desktop_in_files)	\
	mate-user-share.desktop.in.in \
	file-share-properties.ui

CLEANFILES = $(desktop_DATA) $(autostart_DATA)

DISTCLEANFILES = $(schema_DATA)

-include $(top_srcdir)/git.mk
