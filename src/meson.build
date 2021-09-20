gdbus_src = gnome.gdbus_codegen(
  'ua-ubuntu-advantage-generated',
  'com.canonical.UbuntuAdvantage.xml',
  interface_prefix: 'com.canonical',
  namespace: 'Ua',
  autocleanup: 'all'
)

conf = configuration_data()
conf.set_quoted('PROJECT_VERSION', meson.project_version())
configure_file(output: 'config.h',
               configuration: conf)

executable('ua-daemon',
           'main.c', 'ua-service-status.c', 'ua-status.c', 'ua-tool.c',
           gdbus_src,
           dependencies: [gio_dep, json_glib_dep],
           install: true,
           install_dir: libexecdir)

# D-Bus service
service_conf = configuration_data()
service_conf.set('libexecdir', libexecdir)
configure_file(input: 'com.canonical.UbuntuAdvantage.service.in',
               output: 'com.canonical.UbuntuAdvantage.service',
               configuration: service_conf,
               install: true,
               install_dir: join_paths(dbus_datadir, 'system-services'))

# D-Bus configuration
install_data('com.canonical.UbuntuAdvantage.conf', install_dir: join_paths(dbus_datadir, 'system.d'))

# D-Bus interface
install_data('com.canonical.UbuntuAdvantage.xml', install_dir: join_paths(dbus_datadir, 'interfaces'))
