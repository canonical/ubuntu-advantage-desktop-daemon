#include <gio/gio.h>

#include "config.h"
#include "ua-ubuntu-advantage-generated.h"

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

static gboolean ua_ubuntu_advantage_enable(UaUbuntuAdvantage *skeleton,
                                           GDBusMethodInvocation *invocation,
                                           const gchar *service_name,
                                           gpointer user_data) {
  ua_ubuntu_advantage_complete_enable(skeleton, invocation);

  return TRUE;
}

static gboolean ua_ubuntu_advantage_disable(UaUbuntuAdvantage *skeleton,
                                            GDBusMethodInvocation *invocation,
                                            const gchar *service_name,
                                            gpointer user_data) {
  ua_ubuntu_advantage_complete_disable(skeleton, invocation);

  return TRUE;
}

static void on_bus_acquired(GDBusConnection *connection, const gchar *name,
                            gpointer user_data) {
  g_autoptr(GError) error = NULL;
  g_autoptr(UaUbuntuAdvantage) ua = ua_ubuntu_advantage_skeleton_new();
  ua_ubuntu_advantage_set_daemon_version(UA_UBUNTU_ADVANTAGE(ua),
                                         PROJECT_VERSION);
  g_signal_connect(ua, "handle-attach", G_CALLBACK(ua_ubuntu_advantage_attach),
                   NULL);
  g_signal_connect(ua, "handle-detach", G_CALLBACK(ua_ubuntu_advantage_detach),
                   NULL);
  g_signal_connect(ua, "handle-enable", G_CALLBACK(ua_ubuntu_advantage_enable),
                   NULL);
  g_signal_connect(ua, "handle-disable",
                   G_CALLBACK(ua_ubuntu_advantage_disable), NULL);
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

int main(int argc, char **argv) {
  g_autoptr(GMainLoop) loop = g_main_loop_new(NULL, FALSE);

  g_bus_own_name(G_BUS_TYPE_SYSTEM, "com.example.UbuntuAdvantage",
                 G_BUS_NAME_OWNER_FLAGS_NONE, on_bus_acquired, NULL,
                 on_name_lost, loop, NULL);

  g_main_loop_run(loop);

  return EXIT_SUCCESS;
}
