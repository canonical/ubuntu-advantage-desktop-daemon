#include <gio/gio.h>

#include "test-daemon.h"

static void enable_cb(GObject *object, GAsyncResult *result,
                      gpointer user_data) {
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) r =
      g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
  if (r == NULL) {
    g_warning("Failed to enable: %s\n", error->message);
    test_daemon_failure();
    return;
  }

  // Wait for service to change status.
}

static void daemon_ready_cb(GDBusConnection *connection) {
  g_dbus_connection_call(connection, "com.canonical.UbuntuAdvantage",
                         "/com/canonical/UbuntuAdvantage/Services/esm_2dapps",
                         "com.canonical.UbuntuAdvantage.Service", "Enable",
                         g_variant_new("()"), G_VARIANT_TYPE("()"),
                         G_DBUS_CALL_FLAGS_NONE, -1, NULL, enable_cb, NULL);
}

static void service_status_changed_cb(const gchar *service,
                                      const gchar *status) {
  if (strcmp(service, "esm_2dapps") == 0 && strcmp(status, "enabled") == 0) {
    test_daemon_success();
  }
}

int main(int argc, char **argv) {
  return test_daemon_run(FALSE, FALSE, daemon_ready_cb, NULL,
                         service_status_changed_cb);
}
