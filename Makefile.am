SUBDIRS = b3 l3 n3 res src

dist_doc_DATA = COPYING COPYING.font README


dist_appicon_DATA = 3omns.svg

install-data-hook:
	@echo "Updating icon cache..."
	@gtk-update-icon-cache --ignore-theme-index --force $(appicondir) \
			2>&1 > /dev/null || true
	@echo "Done updating icon cache"


if GENERATE_MANPAGES
GENERATED_MANPAGES = 3omns.6
endif

$(GENERATED_MANPAGES): %: %.txt
	$(A2X_BIN) --format manpage $<

man_MANS = $(GENERATED_MANPAGES)
CLEANFILES = $(GENERATED_MANPAGES)
