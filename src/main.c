#include <gio/gio.h>

#include "config.h"
#include "ua-ubuntu-advantage-generated.h"

// Called when a client requests com.canonical.UbuntuAdvantage.Attach().
static gboolean attach_cb(UaUbuntuAdvantage *skeleton,
                          GDBusMethodInvocation *invocation, const gchar *token,
                          gpointer user_data) {
  ua_ubuntu_advantage_complete_attach(skeleton, invocation);

  return TRUE;
}

// Called when a client requests com.canonical.UbuntuAdvantage.Detach().
static gboolean detach_cb(UaUbuntuAdvantage *skeleton,
                          GDBusMethodInvocation *invocation,
                          gpointer user_data) {
  ua_ubuntu_advantage_complete_detach(skeleton, invocation);

  return TRUE;
}

// Called when a client requests com.canonical.UbuntuAdvantage.Enable().
static gboolean enable_cb(UaUbuntuAdvantage *skeleton,
                          GDBusMethodInvocation *invocation,
                          const gchar *service_name, gpointer user_data) {
  ua_ubuntu_advantage_complete_enable(skeleton, invocation);

  return TRUE;
}

// Called when a client requests com.canonical.UbuntuAdvantage.Disable().
static gboolean disable_cb(UaUbuntuAdvantage *skeleton,
                           GDBusMethodInvocation *invocation,
                           const gchar *service_name, gpointer user_data) {
  ua_ubuntu_advantage_complete_disable(skeleton, invocation);

  return TRUE;
}

// Called when the system bus is acquired.
static void bus_acquired_cb(GDBusConnection *connection, const gchar *name,
                            gpointer user_data) {
  g_autoptr(GError) error = NULL;
  g_autoptr(UaUbuntuAdvantage) ua = ua_ubuntu_advantage_skeleton_new();
  ua_ubuntu_advantage_set_daemon_version(UA_UBUNTU_ADVANTAGE(ua),
                                         PROJECT_VERSION);
  g_signal_connect(ua, "handle-attach", G_CALLBACK(attach_cb), NULL);
  g_signal_connect(ua, "handle-detach", G_CALLBACK(detach_cb), NULL);
  g_signal_connect(ua, "handle-enable", G_CALLBACK(enable_cb), NULL);
  g_signal_connect(ua, "handle-disable", G_CALLBACK(disable_cb), NULL);
  if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(ua),
                                        connection, "/", &error)) {
    g_printerr("Failed to export D-Bus object: %s", error->message);
    exit(EXIT_FAILURE);
  }
}

// Called when the com.canonical.UbuntuAdvantage D-Bus name is lost to another
// client.
static void name_lost_cb(GDBusConnection *connection, const gchar *name,
                         gpointer user_data) {
  GMainLoop *loop = user_data;

  g_main_loop_quit(loop);
}

int main(int argc, char **argv) {
  g_autoptr(GMainLoop) loop = g_main_loop_new(NULL, FALSE);

  g_bus_own_name(G_BUS_TYPE_SYSTEM, "com.example.UbuntuAdvantage",
                 G_BUS_NAME_OWNER_FLAGS_NONE, bus_acquired_cb, NULL,
                 name_lost_cb, loop, NULL);

  g_main_loop_run(loop);

  return EXIT_SUCCESS;
}
