bin_PROGRAMS = xlbiff
xlbiff_SOURCES = xlbiff.c mailbox-preview.py

xmhformdir = @xmhformdir@
dist_xmhform_DATA = xlbiff.form

appdefaultdir = @appdefaultdir@
appdefault_DATA = XLbiff
%: %.ad
	cp -- $< $@

nodist_bin_SCRIPTS = mailbox-preview
%: %.py
	cp -- $< $@

MOSTLYCLEANFILES = $(appdefault_DATA) $(nodist_bin_SCRIPTS)
distclean-local:
	rm -rf __pycache__

# "configure" normally sets appdefaultdir for each system with "pkg-config",
# which returns an absolute pathname.  "make distcheck" doesn't use DESTDIR
# doing its test "make install", so it needs some value relative to a
# Makefile variable, so that it doesn't install into the real directory.
DISTCHECK_CONFIGURE_FLAGS = --with-appdefaultdir=\$${datadir}/X11/app-defaults

dist_man1_MANS = xlbiff.man mailbox-preview.man

EXTRA_DIST = README.md README.bulk XLbiff.ad \
    $(bin_SCRIPTS) \
    Bcheck Bscan \
    tests/utilities.sh tests/test-scan-interface.sh tests/test-windowing.sh \
    mailbox-preview-test.py

# Compute mtime of newest project source file, for reproducible dist file.
DATED_SRCS = $(EXTRA_DIST) configure.ac Makefile.am \
    $(xlbiff_SOURCES) $(dist_xmhform_DATA) $(dist_man1_MANS) \
    COPYING NEWS TODO
CLAMP_DATE = $(shell cd $(srcdir) && \
    stat --dereference --format=%Y $(DATED_SRCS) | sort -nr | head -1)

# Rule modified from automake 1.16.1 to write a reproducible dist file.
dist-gzip: distdir
	find $(distdir) -perm /go+w -exec chmod go-w '{}' +
	LANG= LC_COLLATE= GZIP= tar --sort=name \
	    --owner=0 --group=0 --numeric-owner \
	    --mtime=@$(CLAMP_DATE) --clamp-mtime \
	    -chozf $(distdir).tar.gz -- $(distdir)
	$(am__post_remove_distdir)

# aid for building Debian source package
debian-orig-dist: dist
	mv xlbiff-$(VERSION).tar.gz ../xlbiff_$(VERSION).orig.tar.gz
.PHONY: debian-orig-dist
