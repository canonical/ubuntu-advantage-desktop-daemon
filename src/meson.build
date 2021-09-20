gdbus_src = gnome.gdbus_codegen(
  'ua-ubuntu-advantage-generated',
  'com.canonical.UbuntuAdvantage.xml',
  interface_prefix: 'com.canonical',
  namespace: 'Ua',
  autocleanup: 'all'
)

executable('ua-daemon',
           'main.c', 'ua-service-status.c', 'ua-status.c', 'ua-tool.c',
           gdbus_src,
           dependencies: [gio_dep, json_glib_dep])
