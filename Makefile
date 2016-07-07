GITVER = 0.1
DISTROS = trusty

DIST_EXT = tar.xz

WORKDIR = build

PKG := ${shell git show '$(GITVER):configure.ac' \
		| grep '^AC_INIT(' \
		| sed -r 's/^AC_INIT\(\[([^]]+)\].*$$/\1/'}
DEBVER := ${shell git show '$(GITVER):configure.ac' \
		| grep '^AC_INIT(' \
		| sed -r 's/^AC_INIT\(\[[^]]+\], \[([^]]+)\].*$$/\1/'}

GITVER_DATE := $(shell git log -1 --date=iso8601 --format=%cd $(GITVER))
GITVER_PATH := $(shell echo '$(GITVER)' | sed -r 's/[^a-zA-Z0-9.]/_/g')
EXPORTDIR = $(PKG)-git-$(GITVER_PATH)
DISTDIR = $(PKG)-$(DEBVER)
DIST = $(DISTDIR).$(DIST_EXT)
ORIG = $(PKG)_$(DEBVER).orig.$(DIST_EXT)

EXPORT = $(WORKDIR)/export/$(EXPORTDIR)
BUILD = $(WORKDIR)/$(DISTDIR)

MAIN_DISTRO = $(firstword $(DISTROS))
OTHER_DISTROS = $(filter-out $(MAIN_DISTRO), $(DISTROS))

DEBDIR = debian

BUILDDEB = $(BUILD)/$(DEBDIR)

CHANGELOGS = $(WORKDIR)/changelogs
CHANGELOG_PRE = $(CHANGELOGS)/changelog.
MAIN_CHANGELOG = $(CHANGELOG_PRE)$(MAIN_DISTRO)
OTHER_CHANGELOGS = $(OTHER_DISTROS:%=$(CHANGELOG_PRE)%)

PKGFILE_PRE = $(WORKDIR)/$(PKG)_$(DEBVER)-1~
PKGFILE_POST = 1
PKGFILES = $(DISTROS:%=$(PKGFILE_PRE)%$(PKGFILE_POST))

SOURCECHANGES_POST = $(PKGFILE_POST)_source.changes
SOURCECHANGES = $(PKGFILES:%$(PKGFILE_POST)=%$(SOURCECHANGES_POST))

SOURCECHECKS = $(DISTROS:%=%-sourcecheck)

SOURCEDSC_POST = $(PKGFILE_POST).dsc
SOURCEDSCS = $(PKGFILES:%$(PKGFILE_POST)=%$(SOURCEDSC_POST))

PBUILDER = ~/pbuilder
PBUILDERUPDATES = $(DISTROS:%=pbuilder-%-update)


all: sourcescheck

# TODO: deb/debs targets that build the binary packages.  This is complicated
# because debuild always overwrites the .dsc file, so you have to move the
# artifacts elsewhere after building source and before building debs.

# TODO: dput target?  Automatically?

# TODO: behave nicely when the changelog is newer than the GITVER's NEWS file.

sources: $(SOURCECHANGES)
sourcescheck: $(SOURCECHECKS)
	@echo
	@echo "Source packages in $(WORKDIR) have been checked and are ready" \
			"for uploading."


prereqs: $(BUILD)/configure $(BUILDDEB)/changelog

setup: prereqs $(OTHER_CHANGELOGS)
	@echo
	@echo "Updated $(DEBDIR)/changelog and set up $(WORKDIR) for deb" \
			"building, with extra"
	@echo "changelogs in $(CHANGELOGS)."


$(EXPORT).stamp: gitstamp
	@mkdir -p '$(@D)'
	@echo "$(PKG) $(GITVER)'s last commit was on $(GITVER_DATE)." > '$@'
	@touch --date='$(GITVER_DATE)' '$@'
$(EXPORT).tar: $(EXPORT).stamp
	git archive --prefix='$(EXPORTDIR)/' --output='$@' '$(GITVER)'
	touch --date='$(GITVER_DATE)' '$@'
$(EXPORT)/configure.ac $(EXPORT)/NEWS: $(EXPORT).tar
	cd '$(<D)' && tar xf '$(<F)'
	test -f '$@' # configure.ac & NEWS sanity check.
$(EXPORT)/$(DIST): $(EXPORT)/configure.ac
	cd '$(<D)' && autoreconf -i && ./configure && make distcheck
	test -f '$@' # DIST sanity check.

$(WORKDIR)/$(ORIG): $(EXPORT)/$(DIST)
	cp -a '$<' '$@'
$(BUILD)/configure: $(WORKDIR)/$(ORIG)
	cd '$(<D)' && tar xJf '$(<F)'
	test -x '$@' # configure sanity check.
	touch --date="`stat -c %y '$<'`" '$@' # Don't re-untar unless needed.

# TODO: a sanity check target that checks $(DEBDIR)/changelog for having the
# correct version (DEBVER).  If e.g. the NEWS file and existing changelog are
# old, debuild will just fail because it can't find the correctly named .orig
# tarball.  Instead, we should just use dch to add "Unknown changes" to a new
# entry with the correct version.
$(DEBDIR)/changelog: $(EXPORT)/NEWS
	test -n '$(PKG)' -a -n '$(DEBVER)' # PKG & DEBVER sanity check.
	check='$(PKG) $(DEBVER) '; \
		actual=$$(head -n1 '$<' | cut -c-"`expr length "$$check"`"); \
		test "$$check" = "$$actual" # NEWS format sanity check.
	./news2changelog '$(MAIN_DISTRO)' < '$<'
	check='$(PKG) ($(DEBVER)-'; \
		actual=$$(head -n1 '$@' | cut -c-"`expr length "$$check"`"); \
		test "$$check" = "$$actual" # Gen'd changelog sanity check.
$(MAIN_CHANGELOG): $(DEBDIR)/changelog
	! echo '$(MAIN_DISTRO)' | grep -Eq '[^a-zA-Z0-9_]' # Check MAIN_DISTRO.
	mkdir -p '$(@D)'
	cp -a '$<' '$@'
$(OTHER_CHANGELOGS): $(MAIN_CHANGELOG)
	! echo '$(@:$(CHANGELOG_PRE)%=%)' \
			| grep -Eq '[^a-zA-Z0-9_]' # OTHER_DISTROS sanity.
	sed -r '1 s/$(MAIN_DISTRO)/$(@:$(CHANGELOG_PRE)%=%)/g' < '$<' > '$@'

$(BUILDDEB)/changelog: $(DEBDIR)/changelog
	mkdir -p '$(dir $(@D))'
	cp -a '$(<D)' '$(dir $(@D))'
	test -f '$@' # changelog sanity check.

$(SOURCECHANGES): prereqs
$(SOURCECHANGES): $(PKGFILE_PRE)%$(SOURCECHANGES_POST) : $(CHANGELOG_PRE)%
	cp -a '$(CHANGELOG_PRE)$*' '$(BUILDDEB)/changelog'
	cd '$(BUILD)' && debuild -S
	test -f '$@' # Source sanity check.

# TODO: this won't be right if we build any binary debs.
$(SOURCEDSCS): %$(SOURCEDSC_POST) : %$(SOURCECHANGES_POST)

$(PBUILDERUPDATES): pbuilder-%-update :
	test -f $(PBUILDER)'/$*-base.tgz' \
			&& pbuilder-dist '$*' update \
			|| pbuilder-dist '$*' create
	test -f $(PBUILDER)'/$*-base.tgz' # pbuilder-dist sanity check.

$(SOURCECHECKS): %-sourcecheck : pbuilder-%-update
$(SOURCECHECKS): %-sourcecheck : $(PKGFILE_PRE)%$(SOURCEDSC_POST)
	pbuilder-dist '$*' build '$<'


.PHONY: all gitstamp $(PBUILDERUPDATES) prereqs setup $(SOURCECHECKS) sources \
		sourcescheck
