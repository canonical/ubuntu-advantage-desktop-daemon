#pragma once

#include <gio/gio.h>

void ua_check_authorization(const gchar *action_id,
                            GDBusMethodInvocation *invocation,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer callback_data);

gboolean ua_check_authorization_finish(GAsyncResult *result, GError **error);
