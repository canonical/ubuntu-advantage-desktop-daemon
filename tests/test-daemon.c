#include <gio/gio.h>
#include <json-glib/json-glib.h>
#include <stdlib.h>
#include <string.h>

#include "config.h"
#include "test-daemon.h"

static TestDaemonReadyFunction ready_callback;
static TestDaemonAttachedChangedFunction attached_changed_callback;
static TestDaemonServiceStatusChangedFunction service_status_changed_callback;
static GMainLoop *loop = NULL;
static gchar *temp_dir = NULL;
static gchar *status_path = NULL;
static GDBusConnection *connection = NULL;
static pid_t bus_pid = -1;
static gchar *daemon_dbus_name = NULL;
static int exit_result = EXIT_SUCCESS;

static void cleanup() {
  kill(bus_pid, SIGTERM);

  if (status_path != NULL) {
    unlink(status_path);
  }
  if (temp_dir != NULL) {
    rmdir(temp_dir);
  }
}

static void dbus_signal_cb(GDBusConnection *connection,
                           const gchar *sender_name, const gchar *object_path,
                           const gchar *interface_name,
                           const gchar *signal_name, GVariant *parameters,
                           gpointer user_data) {
  if (g_strcmp0(sender_name, daemon_dbus_name) == 0) {
    if (strcmp(object_path, "/com/canonical/UbuntuAdvantage/Manager") == 0) {
      if (strcmp(interface_name, "org.freedesktop.DBus.Properties") == 0 &&
          strcmp(signal_name, "PropertiesChanged") == 0) {
        const gchar *properties_interface_name;
        g_autoptr(GVariantIter) changed_properties = NULL;
        g_autoptr(GVariantIter) invalidated_properties = NULL;
        g_variant_get(parameters, "(&sa{sv}as)", &properties_interface_name,
                      &changed_properties, &invalidated_properties);
        const gchar *property_name;
        GVariant *property_value;
        while (g_variant_iter_loop(changed_properties, "{&sv}", &property_name,
                                   &property_value)) {
          if (strcmp(property_name, "Attached") == 0) {
            if (attached_changed_callback != NULL) {
              attached_changed_callback(g_variant_get_boolean(property_value));
            }
          }
        }
      }
    } else if (g_str_has_prefix(object_path,
                                "/com/canonical/UbuntuAdvantage/Services/")) {
      const char *service_name =
          object_path + strlen("/com/canonical/UbuntuAdvantage/Services/");
      if (strcmp(interface_name, "org.freedesktop.DBus.Properties") == 0 &&
          strcmp(signal_name, "PropertiesChanged") == 0) {
        const gchar *properties_interface_name;
        g_autoptr(GVariantIter) changed_properties = NULL;
        g_autoptr(GVariantIter) invalidated_properties = NULL;
        g_variant_get(parameters, "(&sa{sv}as)", &properties_interface_name,
                      &changed_properties, &invalidated_properties);
        const gchar *property_name;
        GVariant *property_value;
        while (g_variant_iter_loop(changed_properties, "{&sv}", &property_name,
                                   &property_value)) {
          if (strcmp(property_name, "Status") == 0) {
            if (service_status_changed_callback != NULL) {
              service_status_changed_callback(
                  service_name, g_variant_get_string(property_value, NULL));
            }
          }
        }
      }
    }
  } else if (strcmp(sender_name, "org.freedesktop.DBus") == 0) {
    if (strcmp(object_path, "/org/freedesktop/DBus") == 0 &&
        strcmp(interface_name, "org.freedesktop.DBus") == 0) {
      if (strcmp(signal_name, "NameOwnerChanged") == 0) {
        const gchar *name, *old_owner, *new_owner;
        g_variant_get(parameters, "(&s&s&s)", &name, &old_owner, &new_owner);
        if (strcmp(name, "com.canonical.UbuntuAdvantage") == 0) {
          daemon_dbus_name = strdup(new_owner);
          ready_callback(connection);
        }
      }
    }
  }
}

static void
polkit_method_call_cb(GDBusConnection *connection, const gchar *sender,
                      const gchar *object_path, const gchar *interface_name,
                      const gchar *method_name, GVariant *parameters,
                      GDBusMethodInvocation *invocation, gpointer user_data) {
  if (strcmp(object_path, "/org/freedesktop/PolicyKit1/Authority") == 0 &&
      strcmp(interface_name, "org.freedesktop.PolicyKit1.Authority") == 0 &&
      strcmp(method_name, "CheckAuthorization") == 0) {
    g_dbus_method_invocation_return_value(
        invocation, g_variant_new("((bba{ss}))", TRUE, TRUE, NULL));
  } else {
    g_dbus_method_invocation_return_dbus_error(
        invocation, "org.freedesktop.DBus.Error.UnknownMethod",
        "Unknown Object/Interface/Method");
  }
}

int test_daemon_run(
    gboolean attached, gboolean esm_apps_enabled,
    TestDaemonReadyFunction ready_function,
    TestDaemonAttachedChangedFunction attached_changed_function,
    TestDaemonServiceStatusChangedFunction service_status_changed_function) {
  ready_callback = ready_function;
  attached_changed_callback = attached_changed_function;
  service_status_changed_callback = service_status_changed_function;

  loop = g_main_loop_new(NULL, FALSE);

  // Start a DBus server for the mock system bus.
  g_autoptr(GError) error = NULL;
  g_autoptr(GSubprocess) bus_subprocess =
      g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE, &error, "dbus-launch",
                       "--binary-syntax", NULL);
  if (bus_subprocess == NULL) {
    g_warning("Failed launch D-Bus daemon: %s", error->message);
    cleanup();
    return EXIT_FAILURE;
  }

  // Read back address and PID.
  gchar bus_stdout[1024];
  size_t bus_stdout_length = 0, address_length;
  while (TRUE) {
    ssize_t n_read = g_input_stream_read(
        g_subprocess_get_stdout_pipe(bus_subprocess),
        bus_stdout + bus_stdout_length, 1024 - bus_stdout_length, NULL, &error);
    if (n_read < 0) {
      g_warning("Failed to read D-Bus daemon address: %s", error->message);
      cleanup();
      return EXIT_FAILURE;
    }
    bus_stdout_length += n_read;
    address_length = strnlen(bus_stdout, bus_stdout_length);
    if (bus_stdout_length >= address_length + 1 + sizeof(pid_t)) {
      break;
    }
  }
  const gchar *client_address = bus_stdout;
  bus_pid = *(pid_t *)(bus_stdout + address_length + 1);

  // Connect to the mock system bus.
  connection = g_dbus_connection_new_for_address_sync(
      client_address,
      G_DBUS_CONNECTION_FLAGS_AUTHENTICATION_CLIENT |
          G_DBUS_CONNECTION_FLAGS_MESSAGE_BUS_CONNECTION,
      NULL, NULL, &error);
  if (connection == NULL) {
    g_warning("Failed to connect to mock system bus: %s", error->message);
    cleanup();
    return EXIT_FAILURE;
  }
  g_dbus_connection_signal_subscribe(connection, NULL, NULL, NULL, NULL, NULL,
                                     G_DBUS_SIGNAL_FLAGS_NONE, dbus_signal_cb,
                                     NULL, NULL);

  // Add a mock Polkit daemon.
  g_autoptr(GVariant) polkit_register_result = g_dbus_connection_call_sync(
      connection, "org.freedesktop.DBus", "/org/freedesktop/DBus",
      "org.freedesktop.DBus", "RequestName",
      g_variant_new("(su)", "org.freedesktop.PolicyKit1", 0),
      G_VARIANT_TYPE("(u)"), G_DBUS_CALL_FLAGS_NONE, -1, NULL, &error);
  if (polkit_register_result == NULL) {
    g_warning("Failed to request Polkit D-Bus name: %s", error->message);
    cleanup();
    return EXIT_FAILURE;
  }
  const char *polkit_introspection_xml =
      "<node>"
      "  <interface name='org.freedesktop.PolicyKit1.Authority'>"
      "    <method name='CheckAuthorization'>"
      "      <arg name='subject' direction='in' type='(sa{sv})'/>"
      "      <arg name='action_id' direction='in' type='s'/>"
      "      <arg name='details' direction='in' type='a{ss}'/>"
      "      <arg name='flags' direction='in' type='u'/>"
      "      <arg name='cancellation_id' direction='in' type='s'/>"
      "      <arg name='result' direction='out' type='(bba{ss})'/>"
      "    </method>"
      "  </interface>"
      "</node>";
  g_autoptr(GDBusNodeInfo) polkit_introspection_data =
      g_dbus_node_info_new_for_xml(polkit_introspection_xml, NULL);
  GDBusInterfaceVTable polkit_vtable = {
      .method_call = polkit_method_call_cb,
  };
  guint polkit_object_id = g_dbus_connection_register_object(
      connection, "/org/freedesktop/PolicyKit1/Authority",
      polkit_introspection_data->interfaces[0], &polkit_vtable, NULL, NULL,
      &error);
  if (polkit_object_id == 0) {
    g_warning("Failed to register mock polkit authority: %s", error->message);
    cleanup();
    return EXIT_FAILURE;
  }

  // Choose a locations to write status.
  temp_dir = g_dir_make_tmp("uad-test-XXXXXX", &error);
  if (temp_dir == NULL) {
    g_warning("Failed to make temporary directory: %s", error->message);
    cleanup();
    return EXIT_FAILURE;
  }
  status_path = g_build_filename(temp_dir, "status.json", NULL);
  g_autoptr(JsonBuilder) builder = json_builder_new();
  json_builder_begin_object(builder);

  json_builder_set_member_name(builder, "attached");
  json_builder_add_boolean_value(builder, attached);

  json_builder_set_member_name(builder, "services");
  json_builder_begin_array(builder);

  json_builder_begin_object(builder);
  json_builder_set_member_name(builder, "name");
  json_builder_add_string_value(builder, "esm-apps");
  json_builder_set_member_name(builder, "description");
  json_builder_add_string_value(builder,
                                "UA Apps: Extended Security Maintenance (ESM)");
  json_builder_set_member_name(builder, "available");
  json_builder_add_string_value(builder, "yes");
  json_builder_set_member_name(builder, "entitled");
  json_builder_add_string_value(builder, "yes");
  json_builder_set_member_name(builder, "status");
  json_builder_add_string_value(builder,
                                esm_apps_enabled ? "enabled" : "disabled");
  json_builder_end_object(builder);

  json_builder_begin_object(builder);
  json_builder_set_member_name(builder, "name");
  json_builder_add_string_value(builder, "livepatch");
  json_builder_set_member_name(builder, "description");
  json_builder_add_string_value(builder, "Canonical Livepatch service");
  json_builder_set_member_name(builder, "available");
  json_builder_add_string_value(builder, "yes");
  json_builder_set_member_name(builder, "entitled");
  json_builder_add_string_value(builder, "no");
  json_builder_set_member_name(builder, "status");
  json_builder_add_string_value(builder, "n/a");
  json_builder_end_object(builder);

  json_builder_begin_object(builder);
  json_builder_set_member_name(builder, "name");
  json_builder_add_string_value(builder, "disabled");
  json_builder_set_member_name(builder, "description");
  json_builder_add_string_value(builder, "A disabled service");
  json_builder_set_member_name(builder, "available");
  json_builder_add_string_value(builder, "no");
  json_builder_end_object(builder);

  json_builder_end_array(builder);

  json_builder_end_object(builder);

  g_autoptr(JsonGenerator) generator = json_generator_new();
  g_autoptr(JsonNode) root = json_builder_get_root(builder);
  json_generator_set_root(generator, root);
  g_autofree gchar *status_json = json_generator_to_data(generator, NULL);

  if (!g_file_set_contents(status_path, status_json, -1, &error)) {
    g_warning("Failed to write initial UA status: %s", error->message);
    cleanup();
    return EXIT_FAILURE;
  }

  // Spawn the daemon.
  g_autoptr(GSubprocessLauncher) launcher =
      g_subprocess_launcher_new(G_SUBPROCESS_FLAGS_NONE);
  g_subprocess_launcher_setenv(launcher, "DBUS_SYSTEM_BUS_ADDRESS",
                               client_address, TRUE);
  g_subprocess_launcher_setenv(launcher, "PATH", TEST_BUILDDIR, TRUE);
  g_subprocess_launcher_setenv(launcher, "MOCK_UA_STATUS_FILE", status_path,
                               TRUE);
  g_autofree gchar *daemon_path = g_build_filename(
      DAEMON_BUILDDIR, "ubuntu-advantage-desktop-daemon", NULL);
  g_autofree gchar *status_path_arg =
      g_strdup_printf("--status-path=%s", status_path);
  g_autoptr(GSubprocess) subprocess = g_subprocess_launcher_spawn(
      launcher, &error, daemon_path, status_path_arg, NULL);
  if (subprocess == NULL) {
    g_warning("Failed launch daemon %s: %s", daemon_path, error->message);
    cleanup();
    return EXIT_FAILURE;
  }

  g_main_loop_run(loop);

  cleanup();

  return exit_result;
}

void test_daemon_failure() {
  exit_result = EXIT_FAILURE;
  g_main_loop_quit(loop);
}

void test_daemon_success() {
  exit_result = EXIT_SUCCESS;
  g_main_loop_quit(loop);
}
