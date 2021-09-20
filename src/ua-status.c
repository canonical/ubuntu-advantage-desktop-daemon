#include "ua-status.h"

struct _UaStatus {
  GObject parent_instance;

  gboolean attached;
  GPtrArray *services;
};

G_DEFINE_TYPE(UaStatus, ua_status, G_TYPE_OBJECT)

static void ua_status_dispose(GObject *object) {
  UaStatus *self = UA_STATUS(object);

  g_clear_pointer(&self->services, g_ptr_array_unref);

  G_OBJECT_CLASS(ua_status_parent_class)->dispose(object);
}

static void ua_status_init(UaStatus *self) {
  self->services = g_ptr_array_new_with_free_func(g_object_unref);
}

static void ua_status_class_init(UaStatusClass *klass) {
  G_OBJECT_CLASS(klass)->dispose = ua_status_dispose;
}

UaStatus *ua_status_new(gboolean attached) {
  UaStatus *self = g_object_new(ua_status_get_type(), NULL);

  self->attached = attached;

  return self;
}

gboolean ua_status_get_attached(UaStatus *self) {
  g_return_val_if_fail(UA_IS_STATUS(self), FALSE);
  return self->attached;
}

GPtrArray *ua_status_get_services(UaStatus *self) {
  g_return_val_if_fail(UA_IS_STATUS(self), NULL);
  return self->services;
}
