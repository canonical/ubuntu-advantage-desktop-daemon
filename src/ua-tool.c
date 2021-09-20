#include <gio/gio.h>
#include <json-glib/json-glib.h>

#include "ua-tool.h"

static void ua_status_parse_cb(GObject *object, GAsyncResult *result,
                               gpointer user_data) {
  JsonParser *parser = JSON_PARSER(object);
  g_autoptr(GTask) task = G_TASK(user_data);

  g_autoptr(GError) error = NULL;
  if (!json_parser_load_from_stream_finish(parser, result, &error)) {
    g_task_return_error(task, error);
    return;
  }
  JsonNode *root = json_parser_get_root(parser);
  if (!JSON_NODE_HOLDS_OBJECT(root)) {
    g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "Invalid JSON returned");
    return;
  }
  JsonObject *status = json_node_get_object(root);

  gboolean attached =
      json_object_get_boolean_member_with_default(status, "attached", FALSE);

  g_task_return_pointer(task, ua_status_new(attached), g_object_unref);
}

static void ua_status_cb(GObject *object, GAsyncResult *result,
                         gpointer user_data) {
  GSubprocess *subprocess = G_SUBPROCESS(object);
  g_autoptr(GTask) task = G_TASK(user_data);

  g_autoptr(GError) error = NULL;
  if (!g_subprocess_wait_finish(subprocess, result, &error)) {
    g_task_return_error(task, error);
    return;
  }

  g_autoptr(JsonParser) parser = json_parser_new();
  json_parser_load_from_stream_async(
      parser, g_subprocess_get_stdout_pipe(subprocess),
      g_task_get_cancellable(task), ua_status_parse_cb, task);
  g_steal_pointer(&task);
}

static void ua_attach_cb(GObject *object, GAsyncResult *result,
                         gpointer user_data) {
  GSubprocess *subprocess = G_SUBPROCESS(object);
  g_autoptr(GTask) task = G_TASK(user_data);

  g_autoptr(GError) error = NULL;
  if (!g_subprocess_wait_finish(subprocess, result, &error)) {
    g_task_return_error(task, error);
    return;
  }

  g_task_return_boolean(task, TRUE);
}

static void ua_detach_cb(GObject *object, GAsyncResult *result,
                         gpointer user_data) {
  GSubprocess *subprocess = G_SUBPROCESS(object);
  g_autoptr(GTask) task = G_TASK(user_data);

  g_autoptr(GError) error = NULL;
  if (!g_subprocess_wait_finish(subprocess, result, &error)) {
    g_task_return_error(task, error);
    return;
  }

  g_task_return_boolean(task, TRUE);
}

static void ua_enable_cb(GObject *object, GAsyncResult *result,
                         gpointer user_data) {
  GSubprocess *subprocess = G_SUBPROCESS(object);
  g_autoptr(GTask) task = G_TASK(user_data);

  g_autoptr(GError) error = NULL;
  if (!g_subprocess_wait_finish(subprocess, result, &error)) {
    g_task_return_error(task, error);
    return;
  }

  g_task_return_boolean(task, TRUE);
}

static void ua_disable_cb(GObject *object, GAsyncResult *result,
                          gpointer user_data) {
  GSubprocess *subprocess = G_SUBPROCESS(object);
  g_autoptr(GTask) task = G_TASK(user_data);

  g_autoptr(GError) error = NULL;
  if (!g_subprocess_wait_finish(subprocess, result, &error)) {
    g_task_return_error(task, error);
    return;
  }

  g_task_return_boolean(task, TRUE);
}

void ua_get_status(GCancellable *cancellable, GAsyncReadyCallback callback,
                   gpointer callback_data) {
  g_autoptr(GTask) task =
      g_task_new(NULL, cancellable, callback, callback_data);

  g_autoptr(GError) error = NULL;
  g_autoptr(GSubprocess) subprocess =
      g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE, &error, "ua", "status",
                       "--format=json", NULL);
  if (subprocess == NULL) {
    g_task_return_error(task, error);
    return;
  }
  g_subprocess_wait_async(subprocess, cancellable, ua_status_cb,
                          g_steal_pointer(&task));
}

UaStatus *ua_get_status_finish(GAsyncResult *result, GError **error) {
  return g_task_propagate_pointer(G_TASK(result), error);
}

void ua_attach(const char *token, GCancellable *cancellable,
               GAsyncReadyCallback callback, gpointer callback_data) {
  g_autoptr(GTask) task =
      g_task_new(NULL, cancellable, callback, callback_data);

  g_autoptr(GError) error = NULL;
  g_autoptr(GSubprocess) subprocess = g_subprocess_new(
      G_SUBPROCESS_FLAGS_STDOUT_PIPE, &error, "ua", "attach", token, NULL);
  if (subprocess == NULL) {
    g_task_return_error(task, error);
    return;
  }
  g_subprocess_wait_async(subprocess, cancellable, ua_attach_cb,
                          g_steal_pointer(&task));
}

gboolean ua_attach_finish(GAsyncResult *result, GError **error) {
  return g_task_propagate_boolean(G_TASK(result), error);
}

void ua_detach(GCancellable *cancellable, GAsyncReadyCallback callback,
               gpointer callback_data) {
  g_autoptr(GTask) task =
      g_task_new(NULL, cancellable, callback, callback_data);

  g_autoptr(GError) error = NULL;
  g_autoptr(GSubprocess) subprocess = g_subprocess_new(
      G_SUBPROCESS_FLAGS_STDOUT_PIPE, &error, "ua", "detach", NULL);
  if (subprocess == NULL) {
    g_task_return_error(task, error);
    return;
  }
  g_subprocess_wait_async(subprocess, cancellable, ua_detach_cb,
                          g_steal_pointer(&task));
}

gboolean ua_detach_finish(GAsyncResult *result, GError **error) {
  return g_task_propagate_boolean(G_TASK(result), error);
}

void ua_enable(const char *service_name, GCancellable *cancellable,
               GAsyncReadyCallback callback, gpointer callback_data) {
  g_autoptr(GTask) task =
      g_task_new(NULL, cancellable, callback, callback_data);

  g_autoptr(GError) error = NULL;
  g_autoptr(GSubprocess) subprocess =
      g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE, &error, "ua", "enable",
                       service_name, NULL);
  if (subprocess == NULL) {
    g_task_return_error(task, error);
    return;
  }
  g_subprocess_wait_async(subprocess, cancellable, ua_enable_cb,
                          g_steal_pointer(&task));
}

gboolean ua_enable_finish(GAsyncResult *result, GError **error) {
  return g_task_propagate_boolean(G_TASK(result), error);
}

void ua_disable(const char *service_name, GCancellable *cancellable,
                GAsyncReadyCallback callback, gpointer callback_data) {
  g_autoptr(GTask) task =
      g_task_new(NULL, cancellable, callback, callback_data);

  g_autoptr(GError) error = NULL;
  g_autoptr(GSubprocess) subprocess =
      g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE, &error, "ua", "disable",
                       service_name, NULL);
  if (subprocess == NULL) {
    g_task_return_error(task, error);
    return;
  }
  g_subprocess_wait_async(subprocess, cancellable, ua_disable_cb,
                          g_steal_pointer(&task));
}

gboolean ua_disable_finish(GAsyncResult *result, GError **error) {
  return g_task_propagate_boolean(G_TASK(result), error);
}
