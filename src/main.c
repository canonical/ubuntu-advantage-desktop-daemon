#include <gio/gio.h>

#include "ua-ubuntu-advantage-generated.h"

#include "ua-tool.h"

static gboolean ua_ubuntu_advantage_attach(UaUbuntuAdvantage *skeleton,
                                           GDBusMethodInvocation *invocation,
                                           const gchar *token,
                                           gpointer user_data) {
  ua_ubuntu_advantage_complete_attach(skeleton, invocation);

  return TRUE;
}

static gboolean ua_ubuntu_advantage_detach(UaUbuntuAdvantage *skeleton,
                                           GDBusMethodInvocation *invocation,
                                           gpointer user_data) {
  ua_ubuntu_advantage_complete_detach(skeleton, invocation);

  return TRUE;
}

static void on_bus_acquired(GDBusConnection *connection, const gchar *name,
                            gpointer user_data) {
  g_autoptr(GError) error = NULL;
  g_autoptr(UaUbuntuAdvantage) ua = ua_ubuntu_advantage_skeleton_new();
  g_signal_connect(ua, "handle-attach", G_CALLBACK(ua_ubuntu_advantage_attach),
                   NULL);
  g_signal_connect(ua, "handle-detach", G_CALLBACK(ua_ubuntu_advantage_detach),
                   NULL);
  if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(ua),
                                        connection, "/", &error)) {
    g_printerr("Failed to export D-Bus object: %s", error->message);
    exit(EXIT_FAILURE);
  }
}

static void on_name_lost(GDBusConnection *connection, const gchar *name,
                         gpointer user_data) {
  GMainLoop *loop = user_data;

  g_main_loop_quit(loop);
}

static void ua_status_cb(GObject *object, GAsyncResult *result,
                         gpointer user_data) {
  g_autoptr(GError) error = NULL;
  g_autoptr(UaStatus) status = ua_get_status_finish(result, &error);
  g_printerr("attached: %s\n", ua_status_get_attached(status) ? "yes" : "no");
  g_printerr("services:\n");
  GPtrArray *services = ua_status_get_services(status);
  for (guint i = 0; i < services->len; i++) {
    UaServiceStatus *service = g_ptr_array_index(services, i);
    g_printerr("  %s\n", ua_service_status_get_name(service));
  }
}

int main(int argc, char **argv) {
  g_autoptr(GMainLoop) loop = g_main_loop_new(NULL, FALSE);

  g_bus_own_name(G_BUS_TYPE_SESSION, "com.example.UbuntuAdvantage",
                 G_BUS_NAME_OWNER_FLAGS_NONE, on_bus_acquired, NULL,
                 on_name_lost, loop, NULL);

  ua_get_status(NULL, ua_status_cb, NULL);

  g_main_loop_run(loop);

  return EXIT_SUCCESS;
}
