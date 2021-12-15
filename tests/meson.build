conf = configuration_data()
conf.set_quoted('DAEMON_BUILDDIR', daemon_builddir)
conf.set_quoted('TEST_BUILDDIR', meson.current_build_dir())
configure_file(output: 'config.h',
               configuration: conf)

test_attach = executable('test-attach',
                         'test-attach.c',
                         'test-daemon.c',
                          dependencies: [gio_dep, json_glib_dep])

test_attach_invalid_token = executable('test-attach-invalid-token',
                                       'test-attach-invalid-token.c',
                                       'test-daemon.c',
                                       dependencies: [gio_dep, json_glib_dep])

test_detach = executable('test-detach',
                         'test-detach.c',
                         'test-daemon.c',
                          dependencies: [gio_dep, json_glib_dep])

test_list_services = executable('test-list-services',
                                'test-list-services.c',
                                'test-daemon.c',
                                dependencies: [gio_dep, json_glib_dep])

test_enable_service = executable('test-enable-service',
                                 'test-enable-service.c',
                                 'test-daemon.c',
                                 dependencies: [gio_dep, json_glib_dep])

test_disable_service = executable('test-disable-service',
                                  'test-disable-service.c',
                                  'test-daemon.c',
                                  dependencies: [gio_dep, json_glib_dep])

executable('ua',
           'mock-ua.c',
           dependencies: [gio_dep, json_glib_dep])

test('Attach', test_attach)
test('Attach - Invalid Token', test_attach_invalid_token)
test('Detach', test_detach)
test('List Services', test_list_services)
test('Enable Service', test_enable_service)
test('Disable Service', test_disable_service)
