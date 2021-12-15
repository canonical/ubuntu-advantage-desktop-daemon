#pragma once

#include <glib-object.h>

G_DECLARE_FINAL_TYPE(UaDaemon, ua_daemon, UA, DAEMON, GObject)

UaDaemon *ua_daemon_new(gboolean replace, const gchar *status_path);

gboolean ua_daemon_start(UaDaemon *daemon, GError **error);
