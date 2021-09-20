#pragma once

#include <glib-object.h>

G_DECLARE_FINAL_TYPE(UaDaemon, ua_daemon, UA, DAEMON, GObject)

UaDaemon *ua_daemon_new(gboolean replace);

gboolean ua_daemon_start(UaDaemon *self, GError **error);
