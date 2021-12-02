#include <gio/gio.h>

#include "ua-tool.h"

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
                            "UA client exited with code %d",
                            g_subprocess_get_exit_status(subprocess));
  } else {
    g_task_return_new_error(task, G_IO_ERROR, G_IO_ERROR_FAILED,
                            "UA client exited with signal %d",
                            g_subprocess_get_term_sig(subprocess));
  }

  return FALSE;
}

// Called when 'ua attach' process completes.
static void ua_attach_cb(GObject *object, GAsyncResult *result,
                         gpointer user_data) {
  GSubprocess *subprocess = G_SUBPROCESS(object);
  g_autoptr(GTask) task = G_TASK(user_data);

  if (wait_finish(subprocess, result, task)) {
    g_task_return_boolean(task, TRUE);
  }
}

// Called when 'ua detach' process completes.
static void ua_detach_cb(GObject *object, GAsyncResult *result,
                         gpointer user_data) {
  GSubprocess *subprocess = G_SUBPROCESS(object);
  g_autoptr(GTask) task = G_TASK(user_data);

  if (wait_finish(subprocess, result, task)) {
    g_task_return_boolean(task, TRUE);
  }
}

// Called when 'ua enable' process completes.
static void ua_enable_cb(GObject *object, GAsyncResult *result,
                         gpointer user_data) {
  GSubprocess *subprocess = G_SUBPROCESS(object);
  g_autoptr(GTask) task = G_TASK(user_data);

  if (wait_finish(subprocess, result, task)) {
    g_task_return_boolean(task, TRUE);
  }
}

// Called when 'ua disable' process completes.
static void ua_disable_cb(GObject *object, GAsyncResult *result,
                          gpointer user_data) {
  GSubprocess *subprocess = G_SUBPROCESS(object);
  g_autoptr(GTask) task = G_TASK(user_data);

  if (wait_finish(subprocess, result, task)) {
    g_task_return_boolean(task, TRUE);
  }
}

// Attach this machine to an Ubuntu Advantage subscription.
void ua_attach(const char *token, GCancellable *cancellable,
               GAsyncReadyCallback callback, gpointer callback_data) {
  g_autoptr(GTask) task =
      g_task_new(NULL, cancellable, callback, callback_data);

  g_autoptr(GError) error = NULL;
  g_autoptr(GSubprocess) subprocess = g_subprocess_new(
      G_SUBPROCESS_FLAGS_STDOUT_PIPE, &error, "ua", "attach", token, NULL);
  if (subprocess == NULL) {
    g_task_return_error(task, g_steal_pointer(&error));
    return;
  }
  g_subprocess_wait_async(subprocess, cancellable, ua_attach_cb,
                          g_steal_pointer(&task));
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
      g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE, &error, "ua", "detach",
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
      g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE, &error, "ua", "enable",
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
      g_subprocess_new(G_SUBPROCESS_FLAGS_STDOUT_PIPE, &error, "ua", "disable",
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
