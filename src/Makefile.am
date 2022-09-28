libexec_PROGRAMS = ubuntu-advantage-desktop-daemon

ubuntu_advantage_desktop_daemon_SOURCES = \
  main.c \
  ua-authorization.c \
  ua-daemon.c \
  ua-service.c \
  ua-status.c \
  ua-status-monitor.c \
  ua-tool.c \
  ua-ubuntu-advantage-generated.c

ua-daemon.c: ua-ubuntu-advantage-generated.h

ua-ubuntu-advantage-generated.c ua-ubuntu-advantage-generated.h: com.canonical.UbuntuAdvantage.xml
	gdbus-codegen --interface-prefix com.canonical --c-namespace Ua --generate-c-code ua-ubuntu-advantage-generated $<
CLEANFILES = ua-ubuntu-advantage-generated.c ua-ubuntu-advantage-generated.h

ubuntu_advantage_desktop_daemon_CFLAGS = \
  $(WARN_CFLAGS) \
  $(UBUNTU_ADVANTAGE_DESKTOP_DAEMON_CFLAGS)

ubuntu_advantage_desktop_daemon_LDADD = \
  $(UBUNTU_ADVANTAGE_DESKTOP_DAEMON_LIBS)

interfacedir = $(datadir)/dbus-1/interfaces
interface_DATA = com.canonical.UbuntuAdvantage.xml

com.canonical.UbuntuAdvantage.service: com.canonical.UbuntuAdvantage.service.in
	sed -e 's|@libexecdir[@]|$(libexecdir)|g' $< >$@

servicedir = $(datadir)/dbus-1/system-services
service_DATA = com.canonical.UbuntuAdvantage.service

dbus_systemdir = $(datadir)/dbus-1/system.d
dbus_system_DATA = com.canonical.UbuntuAdvantage.conf

polkitdir = $(datadir)/polkit-1/actions
polkit_in_files = com.canonical.UbuntuAdvantage.policy.in
polkit_DATA = $(polkit_in_files:.policy.in=.policy)
@INTLTOOL_POLICY_RULE@

ubuntu-advantage-desktop-daemon.service: ubuntu-advantage-desktop-daemon.service.in
	sed -e 's|@libexecdir[@]|$(libexecdir)|g' $< >$@

systemddir = $(libdir)/systemd/system
systemd_DATA = ubuntu-advantage-desktop-daemon.service

EXTRA_DIST = \
  display-manager.xml
