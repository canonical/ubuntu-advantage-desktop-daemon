#include <gio/gio.h>

#include "config.h"
#include "ua-daemon.h"
#include "ua-tool.h"
#include "ua-ubuntu-advantage-generated.h"

struct _UaDaemon {
  GObject parent_instance;

  gboolean replace;
  UaUbuntuAdvantage *ua;
};

G_DEFINE_TYPE(UaDaemon, ua_daemon, G_TYPE_OBJECT)

enum { SIGNAL_QUIT, SIGNAL_LAST };

static guint signals[SIGNAL_LAST] = {0};

typedef struct {
  UaDaemon *self;
  GDBusMethodInvocation *invocation;
} CallbackData;

static CallbackData *callback_data_new(UaDaemon *self,
                                       GDBusMethodInvocation *invocation) {
  CallbackData *data = g_new0(CallbackData, 1);
  data->self = self;
  data->invocation = g_object_ref(invocation);

  return data;
}

static void callback_data_free(CallbackData *data) {
  g_clear_object(&data->invocation);
  g_free(data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(CallbackData, callback_data_free)

// Called when 'ua attach' completes.
static void attach_cb(GObject *object, GAsyncResult *result,
                      gpointer user_data) {
  g_autoptr(CallbackData) data = user_data;
  UaDaemon *self = data->self;

  g_autoptr(GError) error = NULL;
  if (!ua_attach_finish(result, &error)) {
    g_autofree gchar *error_message =
        g_strdup_printf("Failed to attach: %s", error->message);
    g_dbus_method_invocation_return_dbus_error(
        data->invocation, "com.canonical.UbuntuAdvantage.Failed",
        error_message);
    return;
  }

  ua_ubuntu_advantage_complete_attach(self->ua, data->invocation);
}

// Called when a client requests com.canonical.UbuntuAdvantage.Attach().
static gboolean dbus_attach_cb(UaDaemon *self, const gchar *token,
                               GDBusMethodInvocation *invocation) {
  ua_attach(token, NULL, attach_cb, callback_data_new(self, invocation));
  return TRUE;
}

// Called when 'ua detach' completes.
static void detach_cb(GObject *object, GAsyncResult *result,
                      gpointer user_data) {
  g_autoptr(CallbackData) data = user_data;
  UaDaemon *self = data->self;

  g_autoptr(GError) error = NULL;
  if (!ua_detach_finish(result, &error)) {
    g_autofree gchar *error_message =
        g_strdup_printf("Failed to detach: %s", error->message);
    g_dbus_method_invocation_return_dbus_error(
        data->invocation, "com.canonical.UbuntuAdvantage.Failed",
        error_message);
    return;
  }

  ua_ubuntu_advantage_complete_detach(self->ua, data->invocation);
}

// Called when a client requests com.canonical.UbuntuAdvantage.Detach().
static gboolean dbus_detach_cb(UaDaemon *self,
                               GDBusMethodInvocation *invocation) {
  ua_detach(NULL, detach_cb, callback_data_new(self, invocation));
  return TRUE;
}

// Called when 'ua enable' completes.
static void enable_cb(GObject *object, GAsyncResult *result,
                      gpointer user_data) {
  g_autoptr(CallbackData) data = user_data;
  UaDaemon *self = data->self;

  g_autoptr(GError) error = NULL;
  if (!ua_enable_finish(result, &error)) {
    g_autofree gchar *error_message =
        g_strdup_printf("Failed to enable service: %s", error->message);
    g_dbus_method_invocation_return_dbus_error(
        data->invocation, "com.canonical.UbuntuAdvantage.Failed",
        error_message);
    return;
  }

  ua_ubuntu_advantage_complete_enable(self->ua, data->invocation);
}

// Called when a client requests com.canonical.UbuntuAdvantage.Enable().
static gboolean dbus_enable_cb(UaDaemon *self,
                               GDBusMethodInvocation *invocation,
                               const gchar *service_name) {
  ua_enable(service_name, NULL, enable_cb, callback_data_new(self, invocation));
  return TRUE;
}

// Called when 'ua disable' completes.
static void disable_cb(GObject *object, GAsyncResult *result,
                       gpointer user_data) {
  g_autoptr(CallbackData) data = user_data;
  UaDaemon *self = data->self;

  g_autoptr(GError) error = NULL;
  if (!ua_disable_finish(result, &error)) {
    g_autofree gchar *error_message =
        g_strdup_printf("Failed to disable service: %s", error->message);
    g_dbus_method_invocation_return_dbus_error(
        data->invocation, "com.canonical.UbuntuAdvantage.Failed",
        error_message);
    return;
  }

  ua_ubuntu_advantage_complete_disable(self->ua, data->invocation);
}

// Called when a client requests com.canonical.UbuntuAdvantage.Disable().
static gboolean dbus_disable_cb(UaDaemon *self,
                                GDBusMethodInvocation *invocation,
                                const gchar *service_name) {
  ua_disable(service_name, NULL, disable_cb,
             callback_data_new(self, invocation));
  return TRUE;
}

// Called when the system bus is acquired.
static void bus_acquired_cb(GDBusConnection *connection, const gchar *name,
                            gpointer user_data) {
  UaDaemon *self = user_data;

  g_autoptr(GError) error = NULL;
  if (!g_dbus_interface_skeleton_export(G_DBUS_INTERFACE_SKELETON(self->ua),
                                        connection, "/", &error)) {
    g_printerr("Failed to export D-Bus object: %s", error->message);
    exit(EXIT_FAILURE);
  }
}

// Called when the com.canonical.UbuntuAdvantage D-Bus name is lost to another
// client or failed to acquire it.
static void name_lost_cb(GDBusConnection *connection, const gchar *name,
                         gpointer user_data) {
  UaDaemon *self = user_data;
  g_signal_emit(self, signals[SIGNAL_QUIT], 0);
}

static void ua_daemon_dispose(GObject *object) {
  UaDaemon *self = UA_DAEMON(object);

  g_clear_object(&self->ua);

  G_OBJECT_CLASS(ua_daemon_parent_class)->dispose(object);
}

static void ua_daemon_init(UaDaemon *self) {
  self->ua = ua_ubuntu_advantage_skeleton_new();
  ua_ubuntu_advantage_set_daemon_version(UA_UBUNTU_ADVANTAGE(self->ua),
                                         PROJECT_VERSION);
  g_signal_connect_swapped(self->ua, "handle-attach",
                           G_CALLBACK(dbus_attach_cb), self);
  g_signal_connect_swapped(self->ua, "handle-detach",
                           G_CALLBACK(dbus_detach_cb), self);
  g_signal_connect_swapped(self->ua, "handle-enable",
                           G_CALLBACK(dbus_enable_cb), self);
  g_signal_connect_swapped(self->ua, "handle-disable",
                           G_CALLBACK(dbus_disable_cb), self);
}

static void ua_daemon_class_init(UaDaemonClass *klass) {
  G_OBJECT_CLASS(klass)->dispose = ua_daemon_dispose;

  signals[SIGNAL_QUIT] =
      g_signal_new("quit", G_TYPE_FROM_CLASS(G_OBJECT_CLASS(klass)),
                   G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

UaDaemon *ua_daemon_new(gboolean replace) {
  UaDaemon *self = g_object_new(ua_daemon_get_type(), NULL);

  self->replace = replace;

  return self;
}

gboolean ua_daemon_start(UaDaemon *self, GError **error) {
  GBusNameOwnerFlags bus_flags = G_BUS_NAME_OWNER_FLAGS_ALLOW_REPLACEMENT;
  if (self->replace) {
    bus_flags |= G_BUS_NAME_OWNER_FLAGS_REPLACE;
  }
  g_bus_own_name(G_BUS_TYPE_SYSTEM, "com.canonical.UbuntuAdvantage", bus_flags,
                 bus_acquired_cb, NULL, name_lost_cb, self, NULL);
  return TRUE;
}
