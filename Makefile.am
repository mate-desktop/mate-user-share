SUBDIRS = data po src

EXTRA_DIST =			\
	intltool-extract.in	\
	intltool-merge.in	\
	intltool-update.in

DISTCLEANFILES =		\
	intltool-extract	\
	intltool-merge		\
	intltool-update

DISTCHECK_CONFIGURE_FLAGS = --disable-schemas-install --with-cajadir='$${libdir}/caja/extensions-2.0-distcheck'

# Build ChangeLog from GIT history
ChangeLog:
	@if test -f $(top_srcdir)/.git/HEAD; then \
		git log --stat --after="Thu Jun 11 22:19:01 2009" > $@; \
	fi

dist: ChangeLog

.PHONY: ChangeLog


-include $(top_srcdir)/git.mk
