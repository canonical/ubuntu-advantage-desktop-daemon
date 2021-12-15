#include <gio/gio.h>

#include "test-daemon.h"

static void detach_cb(GObject *object, GAsyncResult *result,
                      gpointer user_data) {
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) r =
      g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
  if (r == NULL) {
    g_warning("Failed to detach: %s\n", error->message);
    test_daemon_failure();
    return;
  }

  // Waiting for attached to be set to false.
}

static void attached_changed_cb(gboolean attached) {
  if (attached) {
    g_warning("Attached property not set to FALSE\n");
    test_daemon_failure();
    return;
  }

  test_daemon_success();
}

static void daemon_ready_cb(GDBusConnection *connection) {
  g_dbus_connection_call(connection, "com.canonical.UbuntuAdvantage",
                         "/com/canonical/UbuntuAdvantage/Manager",
                         "com.canonical.UbuntuAdvantage.Manager", "Detach",
                         g_variant_new("()"), G_VARIANT_TYPE("()"),
                         G_DBUS_CALL_FLAGS_NONE, -1, NULL, detach_cb, NULL);
}

int main(int argc, char **argv) {
  return test_daemon_run(TRUE, FALSE, daemon_ready_cb, attached_changed_cb,
                         NULL);
}
