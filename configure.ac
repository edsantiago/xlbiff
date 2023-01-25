AC_INIT([xlbiff], [4.6.4], [ed@edsantiago.com])
AM_INIT_AUTOMAKE(foreign)
AM_MAINTAINER_MODE(enable)
AC_PROG_CC

AC_DEFINE_UNQUOTED(XLBIFF_MAILPATH,
    ["${CONFIG_MAILPATH:-/var/mail/%s}"])

PKG_PROG_PKG_CONFIG
# Look up X Toolkit app-resources directory for this system.
# Overridden in Makefile.am for "make distcheck".
xt_appdefaultdir=$("$PKG_CONFIG" --variable=appdefaultdir xt 2> /dev/null)
# but if that fails, use a reasonable default.
xt_appdefaultdir=${xt_appdefaultdir:-${datadir}/X11/app-defaults}
AC_ARG_WITH(appdefaultdir,
	AS_HELP_STRING([--with-appdefaultdir=DIR],
	  [install directory for app-defaults file]),
	[appdefaultdir="$withval"], [appdefaultdir="${xt_appdefaultdir}"])
AC_SUBST(appdefaultdir)

AC_ARG_WITH(xmhformdir,
	AS_HELP_STRING([--with-xmhformdir=DIR],
	  [install dir for xlbiff.form file [SYSCONFDIR/nmh]]),
	[xmhformdir="$withval"], [xmhformdir="${sysconfdir}/nmh"])
AC_SUBST(xmhformdir)

AC_SEARCH_LIBS([XCloseDisplay], [X11])
AC_SEARCH_LIBS([XRRSelectInput], [Xrandr])
AC_SEARCH_LIBS([XtAppMainLoop], [Xt])
AC_SEARCH_LIBS([commandWidgetClass], [Xaw])
AC_SUBST(NAME,[xlbiff])
AC_CONFIG_FILES([
  Makefile
])
AC_OUTPUT
