#include <gio/gio.h>

#pragma once

typedef void (*TestDaemonReadyFunction)(GDBusConnection *connection);
typedef void (*TestDaemonAttachedChangedFunction)(gboolean attached);
typedef void (*TestDaemonServiceStatusChangedFunction)(const gchar *service,
                                                       const gchar *status);

int test_daemon_run(
    gboolean attached, gboolean esm_apps_enabled,
    TestDaemonReadyFunction ready_function,
    TestDaemonAttachedChangedFunction attached_changed_function,
    TestDaemonServiceStatusChangedFunction service_status_changed_function);

void test_daemon_failure();

void test_daemon_success();
