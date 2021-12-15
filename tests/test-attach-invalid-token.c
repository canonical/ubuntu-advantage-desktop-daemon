#include <gio/gio.h>

#include "test-daemon.h"

static void attach_invalid_token_cb(GObject *object, GAsyncResult *result,
                                    gpointer user_data) {
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) r =
      g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
  if (r != NULL) {
    g_warning("Attached with invalid token\n");
    test_daemon_failure();
    return;
  }

  test_daemon_success();
}

static void daemon_ready_cb(GDBusConnection *connection) {
  g_dbus_connection_call(connection, "com.canonical.UbuntuAdvantage",
                         "/com/canonical/UbuntuAdvantage/Manager",
                         "com.canonical.UbuntuAdvantage.Manager", "Attach",
                         g_variant_new("(s)", "0000"), G_VARIANT_TYPE("()"),
                         G_DBUS_CALL_FLAGS_NONE, -1, NULL,
                         attach_invalid_token_cb, NULL);
}

int main(int argc, char **argv) {
  return test_daemon_run(FALSE, FALSE, daemon_ready_cb, NULL, NULL);
}
