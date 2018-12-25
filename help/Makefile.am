@YELP_HELP_RULES@

HELP_ID = mate-user-share
HELP_FILES = index.docbook legal.xml
HELP_MEDIA = figures/file-sharing-preferences.png \
	figures/sharing-over-the-network.png \
	figures/sharing-over-bluetooth.png \
	figures/receiving-over-bluetooth.png

# Add linguas to be ignored, e.g. IGNORE_HELP_LINGUAS = ca de es fr
IGNORE_HELP_LINGUAS =
HELP_LINGUAS = $(if $(IGNORE_HELP_LINGUAS), \
	$(filter-out $(IGNORE_HELP_LINGUAS),$(subst /,,$(dir $(wildcard */*.po)))), \
	$(subst /,,$(dir $(wildcard */*.po))) )
 

-include $(top_srcdir)/git.mk
