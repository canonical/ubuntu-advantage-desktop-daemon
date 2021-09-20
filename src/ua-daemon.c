#include <gio/gio.h>

#include "config.h"
#include "ua-daemon.h"
#include "ua-tool.h"
#include "ua-ubuntu-advantage-generated.h"

struct _UaDaemon {
  GObject parent_instance;

  gboolean replace;
  GDBusConnection *connection;
  GDBusObjectManagerServer *object_manager;
  UaUbuntuAdvantage *ua;
  GPtrArray *services;
};

G_DEFINE_TYPE(UaDaemon, ua_daemon, G_TYPE_OBJECT)

enum { SIGNAL_QUIT, SIGNAL_LAST };

static guint signals[SIGNAL_LAST] = {0};

typedef struct {
  UaDaemon *self;
  UaUbuntuAdvantageService *service;
  GDBusMethodInvocation *invocation;
} CallbackData;

static CallbackData *callback_data_new(UaDaemon *self,
                                       UaUbuntuAdvantageService *service,
                                       GDBusMethodInvocation *invocation) {
  CallbackData *data = g_new0(CallbackData, 1);
  data->self = self;
  data->service = service != NULL ? g_object_ref(service) : NULL;
  data->invocation = g_object_ref(invocation);

  return data;
}

static void callback_data_free(CallbackData *data) {
  g_clear_object(&data->service);
  g_clear_object(&data->invocation);
  g_free(data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(CallbackData, callback_data_free)

// Called when 'ua enable' completes.
static void enable_cb(GObject *object, GAsyncResult *result,
                      gpointer user_data) {
  g_autoptr(CallbackData) data = user_data;

  g_autoptr(GError) error = NULL;
  if (!ua_enable_finish(result, &error)) {
    g_autofree gchar *error_message =
        g_strdup_printf("Failed to enable service: %s", error->message);
    g_dbus_method_invocation_return_dbus_error(
        data->invocation, "com.canonical.UbuntuAdvantage.Failed",
        error_message);
    return;
  }

  ua_ubuntu_advantage_service_complete_enable(data->service, data->invocation);
}

// Called when a client requests com.canonical.UbuntuAdvantage.Service.Enable().
static gboolean dbus_service_enable_cb(UaDaemon *self,
                                       GDBusMethodInvocation *invocation,
                                       UaUbuntuAdvantageService *service) {
  ua_enable(ua_ubuntu_advantage_service_get_name(service), NULL, enable_cb,
            callback_data_new(self, service, invocation));
  return TRUE;
}

// Called when 'ua disable' completes.
static void disable_cb(GObject *object, GAsyncResult *result,
                       gpointer user_data) {
  g_autoptr(CallbackData) data = user_data;

  g_autoptr(GError) error = NULL;
  if (!ua_disable_finish(result, &error)) {
    g_autofree gchar *error_message =
        g_strdup_printf("Failed to disable service: %s", error->message);
    g_dbus_method_invocation_return_dbus_error(
        data->invocation, "com.canonical.UbuntuAdvantage.Failed",
        error_message);
    return;
  }

  ua_ubuntu_advantage_service_complete_disable(data->service, data->invocation);
}

// Called when a client requests
// com.canonical.UbuntuAdvantage.Service.Disable().
static gboolean dbus_service_disable_cb(UaDaemon *self,
                                        GDBusMethodInvocation *invocation,
                                        UaUbuntuAdvantageService *service) {
  ua_disable(ua_ubuntu_advantage_service_get_name(service), NULL, disable_cb,
             callback_data_new(self, service, invocation));
  return TRUE;
}

// Update D-Bus interface from [status].
static void update_status(UaDaemon *self, UaStatus *status) {
  ua_ubuntu_advantage_set_attached(UA_UBUNTU_ADVANTAGE(self->ua),
                                   ua_status_get_attached(status));

  GPtrArray *services = ua_status_get_services(status);
  for (guint i = 0; i < services->len; i++) {
    UaService *service = g_ptr_array_index(services, i);

    g_autoptr(UaUbuntuAdvantageService) s =
        ua_ubuntu_advantage_service_skeleton_new();
    g_ptr_array_add(self->services, g_object_ref(s));
    g_signal_connect_swapped(s, "handle-enable",
                             G_CALLBACK(dbus_service_enable_cb), self);
    g_signal_connect_swapped(s, "handle-disable",
                             G_CALLBACK(dbus_service_disable_cb), self);
    ua_ubuntu_advantage_service_set_name(UA_UBUNTU_ADVANTAGE_SERVICE(s),
                                         ua_service_get_name(service));
    ua_ubuntu_advantage_service_set_entitled(UA_UBUNTU_ADVANTAGE_SERVICE(s),
                                             ua_service_get_entitled(service));
    ua_ubuntu_advantage_service_set_status(UA_UBUNTU_ADVANTAGE_SERVICE(s),
                                           ua_service_get_status(service));

    g_autofree gchar *escaped_name =
        g_dbus_escape_object_path(ua_service_get_name(service));
    g_autofree gchar *object_path =
        g_strdup_printf("/Services/%s", escaped_name);

    g_autoptr(GDBusObjectSkeleton) o = g_dbus_object_skeleton_new(object_path);
    g_dbus_object_skeleton_add_interface(o, G_DBUS_INTERFACE_SKELETON(s));
    g_dbus_object_manager_server_export(self->object_manager, o);
  }

  g_autoptr(GDateTime) now = g_date_time_new_now_utc();
  g_autofree gchar *last_refresh =
      g_date_time_format(now, "%Y-%m-%dT%H:%M:%SZ");
  ua_ubuntu_advantage_set_last_refresh(UA_UBUNTU_ADVANTAGE(self->ua),
                                       last_refresh);
}

// Called when 'ua status' completes.
static void get_status_cb(GObject *object, GAsyncResult *result,
                          gpointer user_data) {
  g_autoptr(CallbackData) data = user_data;
  UaDaemon *self = data->self;

  g_autoptr(GError) error = NULL;
  g_autoptr(UaStatus) status = ua_get_status_finish(result, &error);
  if (status == NULL) {
    g_autofree gchar *error_message =
        g_strdup_printf("Failed to get status: %s", error->message);
    g_dbus_method_invocation_return_dbus_error(
        data->invocation, "com.canonical.UbuntuAdvantage.Failed",
        error_message);
    return;
  }

  update_status(self, status);

  ua_ubuntu_advantage_complete_refresh_status(self->ua, data->invocation);
}

// Called when startup run of 'ua status' completes.
static void get_initial_status_cb(GObject *object, GAsyncResult *result,
                                  gpointer user_data) {
  UaDaemon *self = user_data;

  g_autoptr(GError) error = NULL;
  g_autoptr(UaStatus) status = ua_get_status_finish(result, &error);
  if (status == NULL) {
    g_warning("Failed to get initial status: %s", error->message);
    return;
  }

  update_status(self, status);
}

// Called when a client requests com.canonical.UbuntuAdvantage.RefreshStatus().
static gboolean dbus_refresh_status_cb(UaDaemon *self,
                                       GDBusMethodInvocation *invocation) {
  ua_get_status(NULL, get_status_cb, callback_data_new(self, NULL, invocation));
  return TRUE;
}

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
static gboolean dbus_attach_cb(UaDaemon *self,
                               GDBusMethodInvocation *invocation,
                               const gchar *token) {
  ua_attach(token, NULL, attach_cb, callback_data_new(self, NULL, invocation));
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
  ua_detach(NULL, detach_cb, callback_data_new(self, NULL, invocation));
  return TRUE;
}

// Called when the system bus is acquired.
static void bus_acquired_cb(GDBusConnection *connection, const gchar *name,
                            gpointer user_data) {
  UaDaemon *self = user_data;

  self->connection = g_object_ref(connection);
  g_dbus_object_manager_server_set_connection(self->object_manager, connection);

  g_autoptr(GDBusObjectSkeleton) o = g_dbus_object_skeleton_new("/");
  g_dbus_object_skeleton_add_interface(o, G_DBUS_INTERFACE_SKELETON(self->ua));
  g_dbus_object_manager_server_export(self->object_manager, o);
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

  g_clear_object(&self->connection);
  g_clear_object(&self->object_manager);
  g_clear_object(&self->ua);
  g_clear_pointer(&self->services, g_ptr_array_unref);

  G_OBJECT_CLASS(ua_daemon_parent_class)->dispose(object);
}

static void ua_daemon_init(UaDaemon *self) {
  self->object_manager = g_dbus_object_manager_server_new("/");
  self->ua = ua_ubuntu_advantage_skeleton_new();
  self->services = g_ptr_array_new_with_free_func(g_object_unref);
  ua_ubuntu_advantage_set_daemon_version(UA_UBUNTU_ADVANTAGE(self->ua),
                                         PROJECT_VERSION);
  g_signal_connect_swapped(self->ua, "handle-refresh-status",
                           G_CALLBACK(dbus_refresh_status_cb), self);
  g_signal_connect_swapped(self->ua, "handle-attach",
                           G_CALLBACK(dbus_attach_cb), self);
  g_signal_connect_swapped(self->ua, "handle-detach",
                           G_CALLBACK(dbus_detach_cb), self);
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

  ua_get_status(NULL, get_initial_status_cb, self);

  return TRUE;
}
