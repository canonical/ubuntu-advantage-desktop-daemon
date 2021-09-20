#pragma once

#include <glib-object.h>

G_DECLARE_FINAL_TYPE(UaService, ua_service, UA, SERVICE, GObject)

UaService *ua_service_new(const gchar *name, const gchar *entitled,
                          const gchar *status);

const gchar *ua_service_get_name(UaService *status);

const gchar *ua_service_get_entitled(UaService *status);

const gchar *ua_service_get_status(UaService *status);
