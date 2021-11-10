#include "ua-service.h"

struct _UaService {
  GObject parent_instance;

  gchar *name;
  gchar *description;
  gchar *entitled;
  gchar *status;
};

G_DEFINE_TYPE(UaService, ua_service, G_TYPE_OBJECT)

static void ua_service_dispose(GObject *object) {
  UaService *self = UA_SERVICE(object);

  g_clear_pointer(&self->name, g_free);
  g_clear_pointer(&self->description, g_free);
  g_clear_pointer(&self->entitled, g_free);
  g_clear_pointer(&self->status, g_free);

  G_OBJECT_CLASS(ua_service_parent_class)->dispose(object);
}

static void ua_service_init(UaService *self) {}

static void ua_service_class_init(UaServiceClass *klass) {
  G_OBJECT_CLASS(klass)->dispose = ua_service_dispose;
}

UaService *ua_service_new(const gchar *name, const gchar *description,
                          const gchar *entitled, const gchar *status) {
  UaService *self = g_object_new(ua_service_get_type(), NULL);

  self->name = g_strdup(name);
  self->description = g_strdup(description);
  self->entitled = g_strdup(entitled);
  self->status = g_strdup(status);

  return self;
}

// Returns the name of the Ubuntu Advantage service.
const gchar *ua_service_get_name(UaService *self) {
  g_return_val_if_fail(UA_IS_SERVICE(self), NULL);
  return self->name;
}

// Returns the description of the Ubuntu Advantage service.
const gchar *ua_service_get_description(UaService *self) {
  g_return_val_if_fail(UA_IS_SERVICE(self), NULL);
  return self->description;
}

// Returns the entitlement to this service.
const gchar *ua_service_get_entitled(UaService *self) {
  g_return_val_if_fail(UA_IS_SERVICE(self), NULL);
  return self->entitled;
}

// Returns the status of this service.
const gchar *ua_service_get_status(UaService *self) {
  g_return_val_if_fail(UA_IS_SERVICE(self), NULL);
  return self->status;
}
