SUBDIRS = b3 l3 n3 res src

dist_noinst_DATA = COPYING.font
dist_doc_DATA = README


appicondir = $(iconthemedir)/scalable/apps

dist_appicon_DATA = 3omns.svg
dist_desktop_DATA = 3omns.desktop

install-data-hook: update-caches
uninstall-hook: update-caches

update-caches:
	@echo "Attempting to update GTK icon cache..."
	@gtk-update-icon-cache -f -t $(iconthemedir) || true
	@echo "Done updating GTK icon cache"
	@echo "Attempting to update desktop file database..."
	@update-desktop-database $(desktopdir) || true
	@echo "Done updating desktop file database"

.PHONY: update-caches


if GENERATE_MANPAGES
GENERATED_MANPAGES = 3omns.6
endif

$(GENERATED_MANPAGES): %: %.txt
	$(A2X_BIN) --format manpage $<

man_MANS = $(GENERATED_MANPAGES)
CLEANFILES = $(GENERATED_MANPAGES)
