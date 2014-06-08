SUBDIRS = b3 l3 n3 res src

dist_doc_DATA = COPYING COPYING.font README


appicondir = $(iconthemedir)/scalable/apps

dist_appicon_DATA = 3omns.svg

install-data-hook:
	@echo "Attempting to update GTK icon cache..."
	@gtk-update-icon-cache -f -t $(iconthemedir) || true
	@echo "Done updating GTK icon cache"


if GENERATE_MANPAGES
GENERATED_MANPAGES = 3omns.6
endif

$(GENERATED_MANPAGES): %: %.txt
	$(A2X_BIN) --format manpage $<

man_MANS = $(GENERATED_MANPAGES)
CLEANFILES = $(GENERATED_MANPAGES)
