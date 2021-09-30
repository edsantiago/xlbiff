bin_PROGRAMS = xlbiff
xlbiff_SOURCES = xlbiff.c

xmhformdir = @xmhformdir@
dist_xmhform_DATA = xlbiff.form

appdefaultdir = @appdefaultdir@
appdefault_DATA = XLbiff
CLEANFILES = $(appdefault_DATA)
%: %.ad
	cp -- $< $@

# "configure" normally sets appdefaultdir for each system with "pkg-config",
# which returns an absolute pathname.  "make distcheck" doesn't use DESTDIR
# doing its test "make install", so it needs some value relative to a
# Makefile variable, so that it doesn't install into the real directory.
DISTCHECK_CONFIGURE_FLAGS = --with-appdefaultdir=\$${datadir}/X11/app-defaults

dist_man1_MANS = xlbiff.man

EXTRA_DIST = README.md README.bulk XLbiff.ad \
    Bcheck Bscan test-scan-interface.sh

# Rule modified from automake 1.16.1 to write a more reproducible dist file.
dist-gzip: distdir
	chmod -R -- go-w $(distdir)
	GZIP= tar --owner=0 --group=0 --numeric-owner \
	    -chozf $(distdir).tar.gz -- \
	    $$(find $(distdir) ! -type d | LANG= sort)
	$(am__post_remove_distdir)

# aid for building Debian source package
debian-orig-dist: dist
	mv xlbiff-$(VERSION).tar.gz ../xlbiff_$(VERSION).orig.tar.gz
.PHONY: debian-orig-dist
