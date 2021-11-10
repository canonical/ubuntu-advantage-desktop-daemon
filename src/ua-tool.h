#pragma once

#include <gio/gio.h>

#include "ua-status.h"

void ua_attach(const char *token, GCancellable *cancellable,
               GAsyncReadyCallback callback, gpointer callback_data);

gboolean ua_attach_finish(GAsyncResult *result, GError **error);

void ua_detach(GCancellable *cancellable, GAsyncReadyCallback callback,
               gpointer callback_data);

gboolean ua_detach_finish(GAsyncResult *result, GError **error);

void ua_enable(const char *service_name, GCancellable *cancellable,
               GAsyncReadyCallback callback, gpointer callback_data);

gboolean ua_enable_finish(GAsyncResult *result, GError **error);

void ua_disable(const char *service_name, GCancellable *cancellable,
                GAsyncReadyCallback callback, gpointer callback_data);

gboolean ua_disable_finish(GAsyncResult *result, GError **error);
