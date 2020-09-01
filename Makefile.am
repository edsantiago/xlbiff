bin_PROGRAMS = xlbiff
xlbiff_SOURCES = xlbiff.c

xmhformdir = @xmhformdir@
dist_xmhform_DATA = xlbiff.form

appdefaultdir = @appdefaultdir@
appdefault_DATA = XLbiff
CLEANFILES = $(appdefault_DATA)
%: %.ad
	cp -- "$<" "$@"

dist_man1_MANS = xlbiff.man

EXTRA_DIST = README.bulk XLbiff.ad Bcheck Bscan
