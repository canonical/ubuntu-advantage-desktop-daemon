AC_INIT([ubuntu-advantage-desktop-daemon],[1.8])
AC_CONFIG_HEADERS([config.h])
AM_INIT_AUTOMAKE([1.12])
AM_MAINTAINER_MODE([enable])
AM_SILENT_RULES([yes])

AC_PROG_CC
AM_PROG_CC_C_O

PKG_CHECK_MODULES([UBUNTU_ADVANTAGE_DESKTOP_DAEMON], [gio-unix-2.0 json-glib-1.0 polkit-gobject-1])

IT_PROG_INTLTOOL(0.35.0)
AC_SUBST(GETTEXT_PACKAGE, ubuntu-advantage-desktop-daemon)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE", Gettext package)

AC_DEFINE_UNQUOTED(PROJECT_VERSION, "$VERSION", Project version)

AC_CONFIG_FILES([
Makefile
src/Makefile
po/Makefile.in
])
AC_OUTPUT
