#include <gio/gio.h>

#include "config.h"
#include "ua-tool.h"
#include "ua-ubuntu-advantage-generated.h"

typedef struct {
  UaUbuntuAdvantage *ua;
  GDBusMethodInvocation *invocation;
} CallbackData;

static CallbackData *callback_data_new(UaUbuntuAdvantage *ua,
                                       GDBusMethodInvocation *invocation) {
  CallbackData *data = g_new0(CallbackData, 1);
  data->ua = g_object_ref(ua);
  data->invocation = g_object_ref(invocation);

  return data;
}

static void callback_data_free(CallbackData *data) {
  g_clear_object(&data->ua);
  g_clear_object(&data->invocation);
  g_free(data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(CallbackData, callback_data_free)

// Called when 'ua attach' completes.
static void attach_cb(GObject *object, GAsyncResult *result,
                      gpointer user_data) {
  g_autoptr(CallbackData) data = user_data;

  g_autoptr(GError) error = NULL;
  if (!ua_attach_finish(result, &error)) {
    return;
  }

  ua_ubuntu_advantage_complete_attach(data->ua, data->invocation);
}

// Called when a client requests com.canonical.UbuntuAdvantage.Attach().
static gboolean dbus_attach_cb(UaUbuntuAdvantage *ua,
                               GDBusMethodInvocation *invocation,
                               const gchar *token, gpointer user_data) {
  ua_attach(token, NULL, attach_cb, callback_data_new(ua, invocation));
  return TRUE;
}

// Called when 'ua detach' completes.
static void detach_cb(GObject *object, GAsyncResult *result,
                      gpointer user_data) {
  g_autoptr(CallbackData) data = user_data;

  g_autoptr(GError) error = NULL;
  if (!ua_detach_finish(result, &error)) {
    return;
  }

  ua_ubuntu_advantage_complete_detach(data->ua, data->invocation);
}

// Called when a client requests com.canonical.UbuntuAdvantage.Detach().
static gboolean dbus_detach_cb(UaUbuntuAdvantage *ua,
                               GDBusMethodInvocation *invocation,
                               gpointer user_data) {
  ua_detach(NULL, detach_cb, callback_data_new(ua, invocation));
  return TRUE;
}

// Called when 'ua enable' completes.
static void enable_cb(GObject *object, GAsyncResult *result,
                      gpointer user_data) {
  g_autoptr(CallbackData) data = user_data;

  g_autoptr(GError) error = NULL;
  if (!ua_enable_finish(result, &error)) {
    return;
  }

  ua_ubuntu_advantage_complete_enable(data->ua, data->invocation);
}

// Called when a client requests com.canonical.UbuntuAdvantage.Enable().
static gboolean dbus_enable_cb(UaUbuntuAdvantage *ua,
                               GDBusMethodInvocation *invocation,
                               const gchar *service_name, gpointer user_data) {
  ua_enable(service_name, NULL, enable_cb, callback_data_new(ua, invocation));
  return TRUE;
}

// Called when 'ua disable' completes.
static void disable_cb(GObject *object, GAsyncResult *result,
                       gpointer user_data) {
  g_autoptr(CallbackData) data = user_data;

  g_autoptr(GError) error = NULL;
  if (!ua_disable_finish(result, &error)) {
    return;
  }

  ua_ubuntu_advantage_complete_disable(data->ua, data->invocation);
}

// Called when a client requests com.canonical.UbuntuAdvantage.Disable().
static gboolean dbus_disable_cb(UaUbuntuAdvantage *ua,
                                GDBusMethodInvocation *invocation,
                                const gchar *service_name, gpointer user_data) {
  ua_disable(service_name, NULL, disable_cb, callback_data_new(ua, invocation));
  return TRUE;
}

// Called when the system bus is acquired.
static void bus_acquired_cb(GDBusConnection *connection, const gchar *name,
                            gpointer user_data) {
  g_autoptr(GError) error = NULL;
  g_autoptr(UaUbuntuAdvantage) ua = ua_ubuntu_advantage_skeleton_new();
  ua_ubuntu_advantage_set_daemon_version(UA_UBUNTU_ADVANTAGE(ua),
                                         PROJECT_VERSION);
  g_signal_connect(ua, "handle-attach", G_CALLBACK(dbus_attach_cb), NULL);
  g_signal_connect(ua, "handle-detach", G_CALLBACK(dbus_detach_cb), NULL);
  g_signal_connect(ua, "handle-enable", G_CALLBACK(dbus_enable_cb), NULL);
  g_signal_connect(ua, "handle-disable", G_CALLBACK(dbus_disable_cb), NULL);
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
