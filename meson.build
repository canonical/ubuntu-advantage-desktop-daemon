project(
  'ua-daemon', 'c',
  version : '0.1',
  license : 'GPL3',
  meson_version : '>= 0.53.0'
)

gnome = import('gnome')

gio_dep = dependency('gio-unix-2.0')
json_glib_dep = dependency('json-glib-1.0')

libexecdir = join_paths(get_option('prefix'), get_option('libexecdir'))
datadir = get_option('datadir')
dbus_datadir = join_paths(datadir, 'dbus-1')

subdir('src')
