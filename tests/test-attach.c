#include <gio/gio.h>

#include "test-daemon.h"

static void attach_cb(GObject *object, GAsyncResult *result,
                      gpointer user_data) {
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) r =
      g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
  if (r == NULL) {
    g_warning("Failed to attach: %s\n", error->message);
    test_daemon_failure();
    return;
  }

  // Waiting for attached to be set to true.
}

static void get_attached_cb(GObject *object, GAsyncResult *result,
                            gpointer user_data) {
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) r =
      g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
  if (r == NULL) {
    g_warning("Failed to get Attached property: %s\n", error->message);
    test_daemon_failure();
    return;
  }

  g_autoptr(GVariant) attached_variant = NULL;
  g_variant_get(r, "(v)", &attached_variant);
  gboolean attached = g_variant_get_boolean(attached_variant);

  if (attached == TRUE) {
    g_warning("Attached property already set to TRUE\n");
    test_daemon_failure();
    return;
  }

  g_dbus_connection_call(G_DBUS_CONNECTION(object),
                         "com.canonical.UbuntuAdvantage",
                         "/com/canonical/UbuntuAdvantage/Manager",
                         "com.canonical.UbuntuAdvantage.Manager", "Attach",
                         g_variant_new("(s)", "1234"), G_VARIANT_TYPE("()"),
                         G_DBUS_CALL_FLAGS_NONE, -1, NULL, attach_cb, &error);
  g_assert_no_error(error);
}

static void daemon_ready_cb(GDBusConnection *connection) {
  g_autoptr(GError) error = NULL;
  g_dbus_connection_call(connection, "com.canonical.UbuntuAdvantage",
                         "/com/canonical/UbuntuAdvantage/Manager",
                         "org.freedesktop.DBus.Properties", "Get",
                         g_variant_new("(ss)",
                                       "com.canonical.UbuntuAdvantage.Manager",
                                       "Attached"),
                         G_VARIANT_TYPE("(v)"), G_DBUS_CALL_FLAGS_NONE, -1,
                         NULL, get_attached_cb, &error);
  g_assert_no_error(error);
}

static void attached_changed_cb(gboolean attached) {
  if (!attached) {
    g_warning("Attached property not set to TRUE\n");
    test_daemon_failure();
    return;
  }

  test_daemon_success();
}

int main(int argc, char **argv) {
  return test_daemon_run(FALSE, FALSE, daemon_ready_cb, attached_changed_cb,
                         NULL);
}
