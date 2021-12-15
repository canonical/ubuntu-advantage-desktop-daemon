#pragma once

#include <glib-object.h>

#include "ua-status.h"

G_DECLARE_FINAL_TYPE(UaStatusMonitor, ua_status_monitor, UA, STATUS_MONITOR,
                     GObject)

UaStatusMonitor *ua_status_monitor_new(const char *filename);

gboolean ua_status_monitor_start(UaStatusMonitor *monitor, GError **error);

UaStatus *ua_status_monitor_get_status(UaStatusMonitor *monitor);
