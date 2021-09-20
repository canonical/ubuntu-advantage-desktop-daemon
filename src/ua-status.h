#pragma once

#include <glib-object.h>

#include "ua-service-status.h"

G_DECLARE_FINAL_TYPE(UaStatus, ua_status, UA, STATUS, GObject)

UaStatus *ua_status_new(gboolean attached, GPtrArray *services);

gboolean ua_status_get_attached(UaStatus *status);

GPtrArray *ua_status_get_services(UaStatus *status);
