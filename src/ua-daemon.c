#include <gio/gio.h>

#include "config.h"
#include "ua-authorization.h"
#include "ua-daemon.h"
#include "ua-status-monitor.h"
#include "ua-tool.h"
#include "ua-ubuntu-advantage-generated.h"

struct _UaDaemon {
  GObject parent_instance;

  gboolean replace;
  GDBusConnection *connection;
  GDBusObjectManagerServer *object_manager;
  UaUbuntuAdvantage *ua;
  UaStatusMonitor *status_monitor;
  GPtrArray *services;
};

G_DEFINE_TYPE(UaDaemon, ua_daemon, G_TYPE_OBJECT)

enum { SIGNAL_QUIT, SIGNAL_LAST };

static guint signals[SIGNAL_LAST] = {0};

typedef struct {
  UaDaemon *self;
  GDBusMethodInvocation *invocation;
  gchar *token;
} CallbackData;

static CallbackData *callback_data_new(UaDaemon *self,
                                       GDBusMethodInvocation *invocation,
                                       const gchar *token) {
  CallbackData *data = g_new0(CallbackData, 1);
  data->self = self;
  data->invocation = g_object_ref(invocation);
  data->token = g_strdup(token);

  return data;
}

static void callback_data_free(CallbackData *data) {
  g_clear_object(&data->invocation);
  g_clear_pointer(&data->token, g_free);
  g_free(data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(CallbackData, callback_data_free)

typedef struct {
  UaDaemon *self;
  GDBusMethodInvocation *invocation;
  UaUbuntuAdvantageService *service;
} ServiceCallbackData;

static ServiceCallbackData *
service_callback_data_new(UaDaemon *self, GDBusMethodInvocation *invocation,
                          UaUbuntuAdvantageService *service) {
  ServiceCallbackData *data = g_new0(ServiceCallbackData, 1);
  data->self = self;
  data->invocation = g_object_ref(invocation);
  data->service = g_object_ref(service);

  return data;
}

static void service_callback_data_free(ServiceCallbackData *data) {
  g_clear_object(&data->invocation);
  g_clear_object(&data->service);
  g_free(data);
}

G_DEFINE_AUTOPTR_CLEANUP_FUNC(ServiceCallbackData, service_callback_data_free)

// Called when 'ua enable' completes.
static void enable_cb(GObject *object, GAsyncResult *result,
                      gpointer user_data) {
  g_autoptr(ServiceCallbackData) data = user_data;

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

// Called when result of checking authorization for service enablement
// completes.
static void auth_service_enable_cb(GObject *object, GAsyncResult *result,
                                   gpointer user_data) {
  g_autoptr(ServiceCallbackData) data = user_data;

  g_autoptr(GError) error = NULL;
  if (!ua_check_authorization_finish(result, &error)) {
    g_dbus_method_invocation_return_dbus_error(
        data->invocation, "com.canonical.UbuntuAdvantage.AuthFailed",
        error->message);
    return;
  }

  ua_enable(ua_ubuntu_advantage_service_get_name(data->service), NULL,
            enable_cb, data);
  g_steal_pointer(&data);
}

// Called when a client requests com.canonical.UbuntuAdvantage.Service.Enable().
static gboolean dbus_service_enable_cb(UaDaemon *self,
                                       GDBusMethodInvocation *invocation,
                                       UaUbuntuAdvantageService *service) {
  ua_check_authorization("com.canonical.UbuntuAdvantage.enable-service",
                         invocation, NULL, auth_service_enable_cb,
                         service_callback_data_new(self, invocation, service));
  return TRUE;
}

// Called when 'ua disable' completes.
static void disable_cb(GObject *object, GAsyncResult *result,
                       gpointer user_data) {
  g_autoptr(ServiceCallbackData) data = user_data;

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

// Called when result of checking authorization for service disablement
// completes.
static void auth_service_disable_cb(GObject *object, GAsyncResult *result,
                                    gpointer user_data) {
  g_autoptr(ServiceCallbackData) data = user_data;

  g_autoptr(GError) error = NULL;
  if (!ua_check_authorization_finish(result, &error)) {
    g_dbus_method_invocation_return_dbus_error(
        data->invocation, "com.canonical.UbuntuAdvantage.AuthFailed",
        error->message);
    return;
  }

  ua_disable(ua_ubuntu_advantage_service_get_name(data->service), NULL,
             disable_cb, data);
  g_steal_pointer(&data);
}

// Called when a client requests
// com.canonical.UbuntuAdvantage.Service.Disable().
static gboolean dbus_service_disable_cb(UaDaemon *self,
                                        GDBusMethodInvocation *invocation,
                                        UaUbuntuAdvantageService *service) {
  ua_check_authorization("com.canonical.UbuntuAdvantage.disable-service",
                         invocation, NULL, auth_service_disable_cb,
                         service_callback_data_new(self, invocation, service));
  return TRUE;
}

// Escape [s] so that it can be used in a D-Bus object path.
// This implements g_dbus_escape_object_path, which requires glib 2.68
static gchar *escape_object_path(const gchar *s) {
  GString *escaped = g_string_new("");
  for (const gchar *c = s; *c != '\0'; c++) {
    if (g_ascii_isalnum(*c)) {
      g_string_append_c(escaped, *c);
    } else {
      g_string_append_printf(escaped, "_%02x", *c);
    }
  }
  return g_string_free(escaped, FALSE);
}

// Get the D-Bus object path for a UA service with name [service_name].
static gchar *get_service_object_path(const gchar *service_name) {
  g_autofree gchar *escaped_name = escape_object_path(service_name);
  return g_strdup_printf("/com/canonical/UbuntuAdvantage/Services/%s",
                         escaped_name);
}

// Update fields in [dbus_service] from [service].
static void update_service(UaUbuntuAdvantageService *dbus_service,
                           UaService *service) {
  ua_ubuntu_advantage_service_set_name(dbus_service,
                                       ua_service_get_name(service));
  ua_ubuntu_advantage_service_set_entitled(dbus_service,
                                           ua_service_get_entitled(service));
  ua_ubuntu_advantage_service_set_status(dbus_service,
                                         ua_service_get_status(service));
}

// Update D-Bus interface from [status].
static void update_status(UaDaemon *self, UaStatus *status) {
  ua_ubuntu_advantage_set_attached(UA_UBUNTU_ADVANTAGE(self->ua),
                                   ua_status_get_attached(status));

  // Update existing services or remove them.
  g_autoptr(GPtrArray) existing_services =
      g_ptr_array_new_with_free_func(g_object_unref);
  for (guint i = 0; i < self->services->len; i++) {
    UaUbuntuAdvantageService *dbus_service =
        g_ptr_array_index(self->services, i);
    const gchar *service_name =
        ua_ubuntu_advantage_service_get_name(dbus_service);
    UaService *service = ua_status_get_service(status, service_name);
    if (service != NULL) {
      update_service(dbus_service, service);
      g_ptr_array_add(existing_services, g_object_ref(dbus_service));
    } else {
      g_autofree gchar *object_path = get_service_object_path(service_name);
      g_dbus_object_manager_server_unexport(self->object_manager, object_path);
    }
  }
  g_ptr_array_unref(self->services);
  self->services = g_steal_pointer(&existing_services);

  // Add new services.
  GPtrArray *services = ua_status_get_services(status);
  for (guint i = 0; i < services->len; i++) {
    UaService *service = g_ptr_array_index(services, i);
    const gchar *service_name = ua_service_get_name(service);
    g_autofree gchar *object_path = get_service_object_path(service_name);

    if (g_dbus_object_manager_get_object(
            G_DBUS_OBJECT_MANAGER(self->object_manager), object_path) != NULL) {
      continue;
    }

    g_autoptr(UaUbuntuAdvantageService) dbus_service =
        ua_ubuntu_advantage_service_skeleton_new();
    g_ptr_array_add(self->services, g_object_ref(dbus_service));
    g_signal_connect_swapped(dbus_service, "handle-enable",
                             G_CALLBACK(dbus_service_enable_cb), self);
    g_signal_connect_swapped(dbus_service, "handle-disable",
                             G_CALLBACK(dbus_service_disable_cb), self);
    update_service(dbus_service, service);

    g_autoptr(GDBusObjectSkeleton) o = g_dbus_object_skeleton_new(object_path);
    g_dbus_object_skeleton_add_interface(
        o, G_DBUS_INTERFACE_SKELETON(dbus_service));
    g_dbus_object_manager_server_export(self->object_manager, o);
  }
}

// Called when the UA status is changed.
static void status_changed_cb(UaDaemon *self) {
  update_status(self, ua_status_monitor_get_status(self->status_monitor));
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

// Called when result of checking authorization for attach completes.
static void auth_attach_cb(GObject *object, GAsyncResult *result,
                           gpointer user_data) {
  g_autoptr(CallbackData) data = user_data;

  g_autoptr(GError) error = NULL;
  if (!ua_check_authorization_finish(result, &error)) {
    g_dbus_method_invocation_return_dbus_error(
        data->invocation, "com.canonical.UbuntuAdvantage.AuthFailed",
        error->message);
    return;
  }

  ua_attach(data->token, NULL, attach_cb, data);
  g_steal_pointer(&data);
}

// Called when a client requests com.canonical.UbuntuAdvantage.Attach().
static gboolean dbus_attach_cb(UaDaemon *self,
                               GDBusMethodInvocation *invocation,
                               const gchar *token) {
  ua_check_authorization("com.canonical.UbuntuAdvantage.attach", invocation,
                         NULL, auth_attach_cb,
                         callback_data_new(self, invocation, token));
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

// Called when result of checking authorization for detach completes.
static void auth_detach_cb(GObject *object, GAsyncResult *result,
                           gpointer user_data) {
  g_autoptr(CallbackData) data = user_data;

  g_autoptr(GError) error = NULL;
  if (!ua_check_authorization_finish(result, &error)) {
    g_dbus_method_invocation_return_dbus_error(
        data->invocation, "com.canonical.UbuntuAdvantage.AuthFailed",
        error->message);
    return;
  }

  ua_detach(NULL, detach_cb, g_steal_pointer(&data));
}

// Called when a client requests com.canonical.UbuntuAdvantage.Detach().
static gboolean dbus_detach_cb(UaDaemon *self,
                               GDBusMethodInvocation *invocation) {
  ua_check_authorization("com.canonical.UbuntuAdvantage.detach", invocation,
                         NULL, auth_detach_cb,
                         callback_data_new(self, invocation, NULL));
  return TRUE;
}

// Called when the system bus is acquired.
static void bus_acquired_cb(GDBusConnection *connection, const gchar *name,
                            gpointer user_data) {
  UaDaemon *self = user_data;

  self->connection = g_object_ref(connection);
  g_dbus_object_manager_server_set_connection(self->object_manager, connection);

  g_autoptr(GDBusObjectSkeleton) o =
      g_dbus_object_skeleton_new("/com/canonical/UbuntuAdvantage");
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
  g_clear_object(&self->status_monitor);
  g_clear_pointer(&self->services, g_ptr_array_unref);

  G_OBJECT_CLASS(ua_daemon_parent_class)->dispose(object);
}

static void ua_daemon_init(UaDaemon *self) {
  self->object_manager = g_dbus_object_manager_server_new("/");
  self->ua = ua_ubuntu_advantage_skeleton_new();
  self->status_monitor = ua_status_monitor_new();
  self->services = g_ptr_array_new_with_free_func(g_object_unref);
  ua_ubuntu_advantage_set_daemon_version(UA_UBUNTU_ADVANTAGE(self->ua),
                                         PROJECT_VERSION);
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

  g_signal_connect_swapped(self->status_monitor, "changed",
                           G_CALLBACK(status_changed_cb), self);
  if (!ua_status_monitor_start(self->status_monitor, error)) {
    return FALSE;
  }

  return TRUE;
}
