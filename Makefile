GITVER = master
DISTROS = trusty saucy

DISTEXT = tar.xz

BUILDDIR = build

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
DIST = $(DISTDIR).$(DISTEXT)
ORIG = $(PKG)_$(DEBVER).orig.$(DISTEXT)

BUILDDISTDIR = $(BUILDDIR)/$(DISTDIR)

MAINDISTRO = $(firstword $(DISTROS))
OTHERDISTROS = $(filter-out $(MAINDISTRO), $(DISTROS))

DEBDIR = debian

BUILDDEBDIR = $(BUILDDISTDIR)/$(DEBDIR)

CHANGELOGDIR = $(BUILDDIR)/changelogs
CHANGELOGPRE = $(CHANGELOGDIR)/changelog.
MAINCHANGELOG = $(CHANGELOGPRE)$(MAINDISTRO)
OTHERCHANGELOGS = $(OTHERDISTROS:%=$(CHANGELOGPRE)%)

PKGFILEPRE = $(BUILDDIR)/$(PKG)_$(DEBVER)-1~
PKGFILEPOST = 1
PKGFILES = $(DISTROS:%=$(PKGFILEPRE)%$(PKGFILEPOST))

SOURCEPOST = $(PKGFILEPOST)_source.changes
SOURCES = $(PKGFILES:%$(PKGFILEPOST)=%$(SOURCEPOST))

SOURCESCHECK = $(DISTROS:%=%-sourcecheck)

SOURCEDSCPOST = $(PKGFILEPOST).dsc
SOURCEDSCS = $(PKGFILES:%$(PKGFILEPOST)=%$(SOURCEDSCPOST))

PBUILDERDIR = ~/pbuilder
PBUILDERUPDATES = $(DISTROS:%=pbuilder-%-update)


all: sourcescheck

# TODO: deb/debs targets that build the binary packages.  This is complicated
# because debuild always overwrites the .dsc file, so you have to move the
# artifacts elsewhere after building source and before building debs.

# TODO: dput target?  Automatically?

sources: $(SOURCES)
sourcescheck: $(SOURCESCHECK)
	@echo
	@echo "Source packages in $(BUILDDIR) have been checked and are" \
			"ready for uploading."


prereqs: $(BUILDDISTDIR)/configure $(BUILDDEBDIR)/changelog

setup: prereqs $(OTHERCHANGELOGS)
	@echo
	@echo "Updated $(DEBDIR)/changelog and set up $(BUILDDIR) for deb" \
			"building, with extra"
	@echo "changelogs in $(CHANGELOGDIR)."


$(EXPORTDIR).stamp: gitstamp
	@echo "Git $(GITVER)'s last commit was on $(GITVER_DATE)." > '$@'
	@touch --date='$(GITVER_DATE)' '$@'
$(EXPORTDIR).tar: $(EXPORTDIR).stamp
	git archive --prefix='$(EXPORTDIR)/' --output='$@' '$(GITVER)'
	touch --date='$(GITVER_DATE)' '$@'
$(EXPORTDIR)/configure.ac $(EXPORTDIR)/NEWS: $(EXPORTDIR).tar
	tar xf '$<'
	test -f '$@' # configure.ac & NEWS sanity check.
$(EXPORTDIR)/$(DIST): $(EXPORTDIR)/configure.ac
	cd '$(<D)' && autoreconf -i && ./configure && make distcheck
	test -f '$@' # DIST sanity check.

$(BUILDDIR)/$(ORIG): $(EXPORTDIR)/$(DIST)
	mkdir -p '$(@D)'
	cp -a '$<' '$@'
$(BUILDDISTDIR)/configure: $(BUILDDIR)/$(ORIG)
	cd '$(<D)' && tar xJf '$(<F)'
	test -x '$@' # configure sanity check.
	touch --date="`stat -c %y '$<'`" '$@' # Don't re-untar unless needed.

$(DEBDIR)/changelog: $(EXPORTDIR)/NEWS
	test -n '$(PKG)' -a -n '$(DEBVER)' # PKG & DEBVER sanity check.
	check='$(PKG) $(DEBVER) '; \
		actual=$$(head -n1 '$<' | cut -c-"`expr length "$$check"`"); \
		test "$$check" = "$$actual" # NEWS format sanity check.
	./news2changelog '$(MAINDISTRO)' < '$<'
	check='$(PKG) ($(DEBVER)-'; \
		actual=$$(head -n1 '$@' | cut -c-"`expr length "$$check"`"); \
		test "$$check" = "$$actual" # Gen'd changelog sanity check.
$(BUILDDEBDIR)/changelog: $(DEBDIR)/changelog
	mkdir -p '$(dir $(@D))'
	cp -a '$(<D)' '$(dir $(@D))'
	test -f '$@' # changelog sanity check.
$(MAINCHANGELOG): $(DEBDIR)/changelog
	! echo '$(MAINDISTRO)' | grep -Eq '[^a-zA-Z0-9_]' # MAINDISTRO sanity.
	mkdir -p '$(@D)'
	cp -a '$<' '$@'
$(OTHERCHANGELOGS): $(MAINCHANGELOG)
	! echo '$(@:$(CHANGELOGPRE)%=%)' \
			| grep -Eq '[^a-zA-Z0-9_]' # OTHERDISTROS sanity check.
	sed -r '1 s/$(MAINDISTRO)/$(@:$(CHANGELOGPRE)%=%)/g' < '$<' > '$@'

$(SOURCES): prereqs
$(SOURCES): $(PKGFILEPRE)%$(SOURCEPOST) : $(CHANGELOGPRE)%
	cp -a '$(CHANGELOGPRE)$*' '$(BUILDDEBDIR)/changelog'
	cd '$(BUILDDISTDIR)' && debuild -S
	test -f '$@' # Source sanity check.

# TODO: this won't be right if we build any binary debs.
$(SOURCEDSCS): %$(SOURCEDSCPOST) : %$(SOURCEPOST)

$(PBUILDERUPDATES): pbuilder-%-update :
	test -f $(PBUILDERDIR)'/$*-base.tgz' \
			&& pbuilder-dist '$*' update \
			|| pbuilder-dist '$*' create
	test -f $(PBUILDERDIR)'/$*-base.tgz' # pbuilder-dist sanity check.

$(SOURCESCHECK): %-sourcecheck : pbuilder-%-update
$(SOURCESCHECK): %-sourcecheck : $(PKGFILEPRE)%$(SOURCEDSCPOST)
	pbuilder-dist '$*' build '$<'

clean:
	rm -rf '$(EXPORTDIR)' '$(EXPORTDIR).tar' '$(EXPORTDIR).stamp'
	! test -d '$(BUILDDIR)' && rm -f '$(BUILDDIR)' || true
	@test -d '$(BUILDDIR)' \
			&& echo "\nNot cleaning $(BUILDDIR); \"rm -r" \
					"'$(BUILDDIR)'\" at your discretion." \
			|| true


.PHONY: all clean gitstamp $(PBUILDERUPDATES) prereqs setup sources \
		sourcescheck $(SOURCESCHECK)
