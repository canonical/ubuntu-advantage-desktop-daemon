#include <gio/gio.h>

#include "ua-tool.h"

typedef struct {
  char *config_contents;
  GFile *config_file;
  GFileIOStream *iostream;
} AttachData;

static void attach_data_free(AttachData *data) {
  g_clear_pointer(&data->config_contents, g_free);
  g_clear_object(&data->config_file);
  g_clear_object(&data->iostream);
  g_free(data);
}

// Wait for [subprocess] to complete, and return an error in [task] if it did
// not.
static gboolean wait_finish(GSubprocess *subprocess, GAsyncResult *result,
                            GTask *task) {
  g_autoptr(GError) error = NULL;
  if (!g_subprocess_wait_finish(subprocess, result, &error)) {
    g_task_return_error(task, g_steal_pointer(&error));
    return FALSE;
  }

  if (g_subprocess_get_successful(subprocess)) {
    return TRUE;
  }

  if (g_subprocess_get_if_exited(subprocess)) {
    g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "Pro client exited with code %d",
                            g_subprocess_get_exit_status(subprocess));
  } else {
    g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "Pro client exited with signal %d",
                            g_subprocess_get_term_sig(subprocess));
  }

  return FALSE;
}

// Called when 'pro attach' process completes.
static void ua_attach_cb(GObject *object, GAsyncResult *result,
                         gpointer user_data) {
  GSubprocess *subprocess = G_SUBPROCESS(object);
  g_autoptr(GTask) task = G_TASK(user_data);
  AttachData *attach_data = g_task_get_task_data(task);
  g_autoptr(GFile) config_file = g_steal_pointer(&attach_data->config_file);

  if (wait_finish(subprocess, result, task)) {
    g_task_return_boolean(task, TRUE);
  }

  // We don't really care about of the result of this operation.
  g_file_delete_async(config_file, G_PRIORITY_DEFAULT, NULL, NULL, NULL);
}

// Called when 'pro detach' process completes.
static void ua_detach_cb(GObject *object, GAsyncResult *result,
                         gpointer user_data) {
  GSubprocess *subprocess = G_SUBPROCESS(object);
  g_autoptr(GTask) task = G_TASK(user_data);

  if (wait_finish(subprocess, result, task)) {
    g_task_return_boolean(task, TRUE);
  }
}

// Called when 'pro enable' process completes.
static void ua_enable_cb(GObject *object, GAsyncResult *result,
                         gpointer user_data) {
  GSubprocess *subprocess = G_SUBPROCESS(object);
  g_autoptr(GTask) task = G_TASK(user_data);

  if (wait_finish(subprocess, result, task)) {
    g_task_return_boolean(task, TRUE);
  }
}

// Called when 'pro disable' process completes.
static void ua_disable_cb(GObject *object, GAsyncResult *result,
                          gpointer user_data) {
  GSubprocess *subprocess = G_SUBPROCESS(object);
  g_autoptr(GTask) task = G_TASK(user_data);

  if (wait_finish(subprocess, result, task)) {
    g_task_return_boolean(task, TRUE);
  }
}

// Called when token configuration file if filled.
static void ua_attach_call_client(GObject *object, GAsyncResult *result,
                                  gpointer user_data) {
  GOutputStream *output_stream = G_OUTPUT_STREAM(object);
  g_autoptr(GTask) task = G_TASK(user_data);
  g_autoptr(GError) error = NULL;

  if (!g_output_stream_write_all_finish(output_stream, result, NULL, &error)) {
    g_task_return_error(task, g_steal_pointer(&error));
    return;
  }

  // We don't really care about of the result of this operation.
  g_output_stream_close_async(output_stream, G_PRIORITY_DEFAULT, NULL, NULL,
                              NULL);

  AttachData *attach_data = g_task_get_task_data(task);
  g_autofree char *config_file_path = g_file_get_path(attach_data->config_file);
  g_autoptr(GSubprocess) subprocess =
      g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE, &error, "pro", "attach",
                       "--attach-config", config_file_path, NULL);
  if (subprocess == NULL) {
    g_task_return_error(task, g_steal_pointer(&error));
    return;
  }

  GCancellable *cancellable = g_task_get_cancellable(task);
  g_subprocess_wait_async(subprocess, cancellable, ua_attach_cb,
                          g_steal_pointer(&task));
}

static void write_attach_config_file(GFile *config_file,
                                     GFileIOStream *iostream, GTask *task) {
  const char *token = g_task_get_task_data(task);
  AttachData *attach_data = g_new0(AttachData, 1);
  // See:
  // https://canonical-ubuntu-pro-client.readthedocs-hosted.com/en/latest/howtoguides/how_to_attach_with_config_file/
  attach_data->config_contents = g_strdup_printf("token: %s\n", token);
  attach_data->config_file = g_object_ref(config_file);
  attach_data->iostream = g_object_ref(iostream);

  g_task_set_task_data(task, attach_data, (GDestroyNotify)attach_data_free);

  GCancellable *cancellable = g_task_get_cancellable(task);
  GOutputStream *output_stream =
      g_io_stream_get_output_stream(G_IO_STREAM(iostream));
  g_output_stream_write_all_async(
      output_stream, attach_data->config_contents,
      strlen(attach_data->config_contents), G_PRIORITY_DEFAULT, cancellable,
      ua_attach_call_client, g_steal_pointer(&task));
}

#if GLIB_CHECK_VERSION(2, 74, 0)
// Called when token configuration file is created.
static void ua_token_file_create_cb(GObject *object, GAsyncResult *result,
                                    gpointer user_data) {
  g_autoptr(GTask) task = G_TASK(user_data);
  g_autoptr(GFileIOStream) iostream = NULL;
  g_autoptr(GError) error = NULL;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  g_autoptr(GFile) config_file =
      g_file_new_tmp_finish(result, &iostream, &error);
  G_GNUC_END_IGNORE_DEPRECATIONS
  if (!config_file) {
    g_task_return_error(task, g_steal_pointer(&error));
    return;
  }

  write_attach_config_file(config_file, iostream, g_steal_pointer(&task));
}
#endif /* GLIB_CHECK_VERSION(2, 74, 0) */

// Attach this machine to an Ubuntu Advantage subscription.
void ua_attach(const char *token, GCancellable *cancellable,
               GAsyncReadyCallback callback, gpointer callback_data) {
  static const char *file_template = "ubuntu-pro-config-XXXXXX.yaml";
  g_autoptr(GTask) task =
      g_task_new(NULL, cancellable, callback, callback_data);

  g_task_set_task_data(task, g_strdup(token), g_free);

  // This is safe because the file is going to be owned by the daemon user with
  // readwrite permissions only from the same user.
#if GLIB_CHECK_VERSION(2, 74, 0)
  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  g_file_new_tmp_async(file_template, G_PRIORITY_DEFAULT, cancellable,
                       ua_token_file_create_cb, g_steal_pointer(&task));
  G_GNUC_END_IGNORE_DEPRECATIONS
#else
  g_autoptr(GFileIOStream) iostream = NULL;
  g_autoptr(GFile) config_file = NULL;
  g_autoptr(GError) error = NULL;

  config_file = g_file_new_tmp(file_template, &iostream, &error);
  if (config_file == NULL) {
    g_task_return_error(task, g_steal_pointer(&error));
    return;
  }
  write_attach_config_file(config_file, iostream, g_steal_pointer(&task));
#endif
}

// Complete request started with ua_attach().
gboolean ua_attach_finish(GAsyncResult *result, GError **error) {
  return g_task_propagate_boolean(G_TASK(result), error);
}

// Remove this machine from an Ubuntu Advantage subscription.
void ua_detach(GCancellable *cancellable, GAsyncReadyCallback callback,
               gpointer callback_data) {
  g_autoptr(GTask) task =
      g_task_new(NULL, cancellable, callback, callback_data);

  g_autoptr(GError) error = NULL;
  g_autoptr(GSubprocess) subprocess =
      g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE, &error, "pro", "detach",
                       "--assume-yes", NULL);
  if (subprocess == NULL) {
    g_task_return_error(task, g_steal_pointer(&error));
    return;
  }
  g_subprocess_wait_async(subprocess, cancellable, ua_detach_cb,
                          g_steal_pointer(&task));
}

// Complete request started with ua_detach().
gboolean ua_detach_finish(GAsyncResult *result, GError **error) {
  return g_task_propagate_boolean(G_TASK(result), error);
}

// Enable [service_name] on this machine.
void ua_enable(const char *service_name, GCancellable *cancellable,
               GAsyncReadyCallback callback, gpointer callback_data) {
  g_autoptr(GTask) task =
      g_task_new(NULL, cancellable, callback, callback_data);

  g_autoptr(GError) error = NULL;
  g_autoptr(GSubprocess) subprocess =
      g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE, &error, "pro", "enable",
                       "--assume-yes", service_name, NULL);
  if (subprocess == NULL) {
    g_task_return_error(task, g_steal_pointer(&error));
    return;
  }
  g_subprocess_wait_async(subprocess, cancellable, ua_enable_cb,
                          g_steal_pointer(&task));
}

// Complete request started with ua_enable().
gboolean ua_enable_finish(GAsyncResult *result, GError **error) {
  return g_task_propagate_boolean(G_TASK(result), error);
}

// Disable [service_name] on this machine.
void ua_disable(const char *service_name, GCancellable *cancellable,
                GAsyncReadyCallback callback, gpointer callback_data) {
  g_autoptr(GTask) task =
      g_task_new(NULL, cancellable, callback, callback_data);

  g_autoptr(GError) error = NULL;
  g_autoptr(GSubprocess) subprocess =
      g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE, &error, "pro", "disable",
                       "--assume-yes", service_name, NULL);
  if (subprocess == NULL) {
    g_task_return_error(task, g_steal_pointer(&error));
    return;
  }
  g_subprocess_wait_async(subprocess, cancellable, ua_disable_cb,
                          g_steal_pointer(&task));
}

// Complete request started with ua_disable().
gboolean ua_disable_finish(GAsyncResult *result, GError **error) {
  return g_task_propagate_boolean(G_TASK(result), error);
}
