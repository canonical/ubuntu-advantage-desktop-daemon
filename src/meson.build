gdbus_codegen = find_program('gdbus-codegen')
gdbus_src = custom_target(
  'ua-ubuntu-advantage-generated',
  input: 'com.canonical.UbuntuAdvantage.xml',
  output: [
    'ua-ubuntu-advantage-generated.c',
    'ua-ubuntu-advantage-generated.h',
  ],
  command: [
    gdbus_codegen,
    '--interface-prefix', 'com.canonical',
    '--c-namespace', 'Ua',
    '--generate-c-code', 'src/ua-ubuntu-advantage-generated',
    '@INPUT@',
  ]
)

daemon_builddir = meson.current_build_dir()

conf = configuration_data()
conf.set_quoted('GETTEXT_PACKAGE', 'ubuntu-advantage-desktop-daemon')
conf.set_quoted('PROJECT_VERSION', meson.project_version())
configure_file(output: 'config.h',
               configuration: conf)

ua_daemon = executable('ubuntu-advantage-desktop-daemon',
           'main.c', 'ua-authorization.c', 'ua-daemon.c', 'ua-service.c', 'ua-status.c', 'ua-status-monitor.c', 'ua-tool.c',
           gdbus_src,
           dependencies: [gio_dep, json_glib_dep, polkit_gobject_dep],
           include_directories: include_directories('..'),
           install: true,
           install_dir: libexecdir)

# D-Bus service
service_conf = configuration_data()
service_conf.set('libexecdir', libexecdir)
service_file = configure_file(input: 'com.canonical.UbuntuAdvantage.service.in',
                              output: 'com.canonical.UbuntuAdvantage.service',
                              configuration: service_conf)

install_data(service_file, install_dir: join_paths(dbus_datadir, 'system-services'))

# Systemd unit
systemd = dependency('systemd')
unitdir = systemd.get_pkgconfig_variable('systemdsystemunitdir')

confsystemd = configuration_data()
confsystemd.set('libexecdir', libexecdir)

systemd_unit = configure_file(
  input: 'ubuntu-advantage-desktop-daemon.service.in',
  output: 'ubuntu-advantage-desktop-daemon.service',
  configuration: confsystemd
)

install_data(systemd_unit,
  install_dir: unitdir
)

# D-Bus configuration
install_data('com.canonical.UbuntuAdvantage.conf', install_dir: join_paths(dbus_datadir, 'system.d'))

# D-Bus interface
install_data('com.canonical.UbuntuAdvantage.xml', install_dir: join_paths(dbus_datadir, 'interfaces'))

# Polkit policy
i18n.merge_file(
  input: 'com.canonical.UbuntuAdvantage.policy.in',
  output: 'com.canonical.UbuntuAdvantage.policy',
  data_dirs: po_dir,
  po_dir: po_dir,
  install: true,
  install_dir: policy_dir,
)
