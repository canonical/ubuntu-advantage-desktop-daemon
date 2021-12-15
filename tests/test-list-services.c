#include <gio/gio.h>

#include "test-daemon.h"

static GVariant *get_property(GVariantIter *properties, const gchar *name) {
  const gchar *property_name;
  GVariant *property_value;
  while (g_variant_iter_loop(properties, "{&sv}", &property_name,
                             &property_value)) {
    if (strcmp(property_name, name) == 0) {
      return property_value;
    }
  }

  return NULL;
}

static gboolean get_boolean_property(GVariantIter *properties,
                                     const gchar *name) {
  g_autoptr(GVariant) value = get_property(properties, name);
  return value != NULL ? g_variant_get_boolean(value) : FALSE;
}

static const gchar *get_string_property(GVariantIter *properties,
                                        const gchar *name) {
  g_autoptr(GVariant) value = get_property(properties, name);
  return value != NULL ? g_variant_get_string(value, NULL) : NULL;
}

static void get_managed_objects_cb(GObject *object, GAsyncResult *result,
                                   gpointer user_data) {
  g_autoptr(GError) error = NULL;
  g_autoptr(GVariant) r =
      g_dbus_connection_call_finish(G_DBUS_CONNECTION(object), result, &error);
  if (r == NULL) {
    g_warning("Failed to get managed objects: %s\n", error->message);
    test_daemon_failure();
    return;
  }

  gboolean valid_manager = FALSE, valid_livepatch_service = FALSE,
           valid_esm_apps_service = FALSE;

  g_autoptr(GVariantIter) object_paths_interfaces_and_properties = NULL;
  g_variant_get(r, "(a{oa{sa{sv}}})", &object_paths_interfaces_and_properties);
  const gchar *object_path;
  GVariantIter *interfaces_and_properties;
  while (g_variant_iter_loop(object_paths_interfaces_and_properties,
                             "{&oa{sa{sv}}}", &object_path,
                             &interfaces_and_properties)) {
    const gchar *interface_name;
    GVariantIter *properties;
    while (g_variant_iter_loop(interfaces_and_properties, "{&sa{sv}}",
                               &interface_name, &properties)) {
      if (strcmp(object_path, "/com/canonical/UbuntuAdvantage/Manager") == 0) {
        valid_manager = get_boolean_property(properties, "Attached") == FALSE;
      } else if (strcmp(object_path,
                        "/com/canonical/UbuntuAdvantage/Services/esm_2dapps") ==
                 0) {
        valid_esm_apps_service =
            g_strcmp0(get_string_property(properties, "Name"), "esm-apps") ==
                0 &&
            g_strcmp0(get_string_property(properties, "Description"),
                      "UA Apps: Extended Security Maintenance (ESM)") == 0 &&
            g_strcmp0(get_string_property(properties, "Entitled"), "yes") ==
                0 &&
            g_strcmp0(get_string_property(properties, "Status"), "disabled") ==
                0;
      } else if (strcmp(object_path,
                        "/com/canonical/UbuntuAdvantage/Services/livepatch") ==
                 0) {
        valid_livepatch_service =
            g_strcmp0(get_string_property(properties, "Name"), "livepatch") ==
                0 &&
            g_strcmp0(get_string_property(properties, "Description"),
                      "Canonical Livepatch service") == 0 &&
            g_strcmp0(get_string_property(properties, "Entitled"), "no") == 0 &&
            g_strcmp0(get_string_property(properties, "Status"), "n/a") == 0;
      }
    }
  }

  if (!valid_manager || !valid_livepatch_service || !valid_esm_apps_service) {
    g_warning("Invalid/missing managed objects\n");
    test_daemon_failure();
    return;
  }

  test_daemon_success();
}

static void daemon_ready_cb(GDBusConnection *connection) {
  g_dbus_connection_call(
      connection, "com.canonical.UbuntuAdvantage", "/",
      "org.freedesktop.DBus.ObjectManager", "GetManagedObjects",
      g_variant_new("()"), G_VARIANT_TYPE("(a{oa{sa{sv}}})"),
      G_DBUS_CALL_FLAGS_NONE, -1, NULL, get_managed_objects_cb, NULL);
}

int main(int argc, char **argv) {
  return test_daemon_run(FALSE, FALSE, daemon_ready_cb, NULL, NULL);
}
