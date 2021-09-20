#pragma once

#include <glib-object.h>

G_DECLARE_FINAL_TYPE(UaServiceStatus, ua_service_status, UA, SERVICE_STATUS,
                     GObject);

UaServiceStatus *ua_service_status_new(const gchar *name, const gchar *entitled,
                                       const gchar *status);

const gchar *ua_service_status_get_name(UaServiceStatus *status);

const gchar *ua_service_status_get_entitled(UaServiceStatus *status);

const gchar *ua_service_status_get_status(UaServiceStatus *status);
