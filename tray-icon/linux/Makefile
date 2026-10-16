# Makefile — install the Linux tray apps, or build their .deb and .rpm.
#
#   make install            # into /usr/local (PREFIX=/usr/local), for trying it out
#   make deb                # dist/achma-tray_<v>-1_all.deb   (needs dpkg-deb)
#   make rpm                # dist/achma-tray-<v>-1.noarch.rpm (needs rpmbuild)
#   make packages           # both
#   make check              # prayer times == the page's JavaScript (needs node)
#
# One source tree, two package formats: both packages are built from the same
# `make install` into a staging root, so they can never ship different files.
# Pure Python + GTK 3, so both are architecture-independent (all / noarch).

NAME     := achma-tray
VERSION  := $(shell sed -n 's/^__version__ = "\(.*\)"/\1/p' achma_tray/__init__.py)
RELEASE  := 1
PREFIX   ?= /usr/local
DESTDIR  ?=
SHARE     = $(DESTDIR)$(PREFIX)/share
DOCDIR   ?= $(PREFIX)/share/doc/$(NAME)
# the HTML pages the menus open; from the repo, or from a release tarball's pages/
PAGES    ?= $(if $(wildcard pages/ai-hub.html),pages,../../html file)

APPS     := prayer-tray ai-hub-tray
IDS      := achma-prayer-tray achma-ai-hub-tray

.PHONY: all install uninstall deb rpm packages dist check clean

all:
	@echo "targets: install uninstall deb rpm packages dist check clean"

install:
	install -d "$(SHARE)/$(NAME)/achma_tray/pages" "$(DESTDIR)$(PREFIX)/bin"
	install -m 644 achma_tray/*.py "$(SHARE)/$(NAME)/achma_tray/"
	install -m 644 "$(PAGES)/start-dock.html" "$(PAGES)/ai-hub.html" "$(SHARE)/$(NAME)/achma_tray/pages/"
	for a in $(APPS); do install -m 755 bin/$$a "$(DESTDIR)$(PREFIX)/bin/$$a"; done
	for i in $(IDS); do \
	  install -D -m 644 data/$$i.desktop "$(SHARE)/applications/$$i.desktop"; \
	  install -D -m 644 data/icons/$$i.svg "$(SHARE)/icons/hicolor/scalable/apps/$$i.svg"; \
	done
	install -D -m 644 README.md "$(DESTDIR)$(DOCDIR)/README.md"

uninstall:
	rm -rf "$(SHARE)/$(NAME)" "$(DESTDIR)$(DOCDIR)"
	for a in $(APPS); do rm -f "$(DESTDIR)$(PREFIX)/bin/$$a"; done
	for i in $(IDS); do rm -f "$(SHARE)/applications/$$i.desktop" "$(SHARE)/icons/hicolor/scalable/apps/$$i.svg"; done

# ── .deb ───────────────────────────────────────────────────────────────
deb:
	rm -rf build/deb && mkdir -p build/deb/DEBIAN dist
	$(MAKE) --no-print-directory install DESTDIR="$(CURDIR)/build/deb" PREFIX=/usr
	sed -e 's/@VERSION@/$(VERSION)-$(RELEASE)/' \
	    -e "s/@SIZE@/$$(du -sk build/deb/usr | cut -f1)/" packaging/debian/control > build/deb/DEBIAN/control
	install -m 755 packaging/debian/postinst packaging/debian/postrm build/deb/DEBIAN/
	cd build/deb && find usr -type f -exec md5sum {} + > DEBIAN/md5sums
	dpkg-deb --root-owner-group -Zxz --build build/deb dist/$(NAME)_$(VERSION)-$(RELEASE)_all.deb

# ── .rpm ───────────────────────────────────────────────────────────────
dist:
	rm -rf build/dist && mkdir -p build/dist/$(NAME)-$(VERSION)/pages dist
	cp -r Makefile README.md achma_tray bin data packaging tests build/dist/$(NAME)-$(VERSION)/
	cp "$(PAGES)/start-dock.html" "$(PAGES)/ai-hub.html" build/dist/$(NAME)-$(VERSION)/pages/
	find build/dist -name __pycache__ -prune -exec rm -rf {} +
	tar -C build/dist -czf dist/$(NAME)-$(VERSION).tar.gz $(NAME)-$(VERSION)

rpm: dist
	rm -rf build/rpm && mkdir -p build/rpm/SOURCES
	cp dist/$(NAME)-$(VERSION).tar.gz build/rpm/SOURCES/
	# on a non-RPM distro (building the .rpm from Ubuntu) the rpm database is
	# empty, so BuildRequires: make can never be satisfied there — skip that check
	rpmbuild -bb packaging/$(NAME).spec $$(rpm -q make >/dev/null 2>&1 || echo --nodeps) \
	  --define "_topdir $(CURDIR)/build/rpm" \
	  --define "version_ $(VERSION)" --define "release_ $(RELEASE)" --define "dist %{nil}"
	cp build/rpm/RPMS/noarch/$(NAME)-$(VERSION)-$(RELEASE).noarch.rpm dist/

packages: deb rpm

check:
	python3 tests/check_times.py

clean:
	rm -rf build
	find . -name __pycache__ -prune -exec rm -rf {} +
