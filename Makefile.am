SUBDIRS = b3 l3 n3 res src

dist_noinst_DATA = COPYING.font


appicondir = $(iconthemedir)/scalable/apps

dist_appicon_DATA = 3omns.svg
dist_desktop_DATA = 3omns.desktop

install-data-hook: update-caches
uninstall-hook: update-caches

update-caches:
	gtk-update-icon-cache -f -t "$(iconthemedir)" || true

.PHONY: update-caches


if GENERATE_MANPAGES
GENERATED_MANPAGES = 3omns.6
endif

$(GENERATED_MANPAGES): %: %.txt
	$(AM_V_GEN)$(A2X_BIN) --format manpage $<

man_MANS = $(GENERATED_MANPAGES)
CLEANFILES = $(GENERATED_MANPAGES)
