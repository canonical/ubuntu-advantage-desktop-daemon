#include "ua-service-status.h"

struct _UaServiceStatus {
  GObject parent_instance;

  gchar *name;
  gchar *entitled;
  gchar *status;
};

G_DEFINE_TYPE(UaServiceStatus, ua_service_status, G_TYPE_OBJECT)

static void ua_service_status_dispose(GObject *object) {
  UaServiceStatus *self = UA_SERVICE_STATUS(object);

  g_clear_pointer(&self->name, g_free);
  g_clear_pointer(&self->entitled, g_free);
  g_clear_pointer(&self->status, g_free);

  G_OBJECT_CLASS(ua_service_status_parent_class)->dispose(object);
}

static void ua_service_status_init(UaServiceStatus *self) {}

static void ua_service_status_class_init(UaServiceStatusClass *klass) {
  G_OBJECT_CLASS(klass)->dispose = ua_service_status_dispose;
}

UaServiceStatus *ua_service_status_new(const gchar *name, const gchar *entitled,
                                       const gchar *status) {
  UaServiceStatus *self = g_object_new(ua_service_status_get_type(), NULL);

  self->name = g_strdup(name);
  self->entitled = g_strdup(entitled);
  self->status = g_strdup(status);

  return self;
}

// Returns the name of the Ubuntu Advantage service.
const gchar *ua_service_status_get_name(UaServiceStatus *self) {
  g_return_val_if_fail(UA_IS_SERVICE_STATUS(self), NULL);
  return self->name;
}

// Returns the entitlement to this service.
const gchar *ua_service_status_get_entitled(UaServiceStatus *self) {
  g_return_val_if_fail(UA_IS_SERVICE_STATUS(self), NULL);
  return self->entitled;
}

// Returns the status of this service.
const gchar *ua_service_status_get_status(UaServiceStatus *self) {
  g_return_val_if_fail(UA_IS_SERVICE_STATUS(self), NULL);
  return self->status;
}
