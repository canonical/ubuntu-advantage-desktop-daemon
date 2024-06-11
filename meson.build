project(
  'ubuntu-advantage-desktop-daemon', 'c',
  version : '1.8',
  license : 'GPL3',
  meson_version : '>= 0.45.1'
)

gnome = import('gnome')
i18n = import('i18n')

# We want to target 16.04 for now
glib_minmax_version = '2.48'

glib_version_def = 'GLIB_VERSION_@0@_@1@'.format(
  glib_minmax_version.split('.')[0], glib_minmax_version.split('.')[1])

cc = meson.get_compiler('c')
add_project_arguments(cc.get_supported_arguments([
    '-DGLIB_VERSION_MIN_REQUIRED=' + glib_version_def,
    '-DGLIB_VERSION_MAX_ALLOWED=' + glib_version_def,
  ]),
  language: 'c')

gio_dep = dependency('gio-unix-2.0', version: '>=' + glib_minmax_version)
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
