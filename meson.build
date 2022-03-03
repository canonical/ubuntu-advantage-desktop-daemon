project(
  'ubuntu-advantage-desktop-daemon', 'c',
  version : '1.7',
  license : 'GPL3',
  meson_version : '>= 0.45.1'
)

gnome = import('gnome')
i18n = import('i18n')

gio_dep = dependency('gio-unix-2.0')
json_glib_dep = dependency('json-glib-1.0')
polkit_gobject_dep = dependency('polkit-gobject-1')

datadir = get_option('datadir')
dbus_datadir = join_paths(datadir, 'dbus-1')
libexecdir = join_paths(get_option('prefix'), get_option('libexecdir'))
policy_dir = polkit_gobject_dep.get_pkgconfig_variable('policydir', define_variable: ['prefix', get_option('prefix')])
po_dir = join_paths(meson.current_source_dir(), 'po')

subdir('po')
subdir('src')
subdir('tests')
