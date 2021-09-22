#include <gio/gio.h>
#include <polkit/polkit.h>

#include "ua-authorization.h"

// Called when permission is acquired for this action.
static void permission_acquired_cb(GObject *object, GAsyncResult *result,
                                   gpointer user_data) {
  GPermission *permission = G_PERMISSION(object);
  g_autoptr(GTask) task = G_TASK(user_data);

  g_autoptr(GError) error = NULL;
  if (!g_permission_acquire_finish(permission, result, &error)) {
    g_task_return_error(task, g_steal_pointer(&error));
    return;
  }

  if (!g_permission_get_allowed(permission)) {
    g_task_return_new_error(
        task, G_IO_ERROR, G_IO_ERROR_FAILED, "Not allowed to perform %s",
        polkit_permission_get_action_id(POLKIT_PERMISSION(permission)));
    return;
  }

  g_task_return_boolean(task, TRUE);
}

// Called when permission created.
static void permission_cb(GObject *object, GAsyncResult *result,
                          gpointer user_data) {
  g_autoptr(GTask) task = G_TASK(user_data);

  g_autoptr(GError) error = NULL;
  g_autoptr(GPermission) permission =
      polkit_permission_new_finish(result, &error);
  if (permission == NULL) {
    g_task_return_error(task, g_steal_pointer(&error));
    return;
  }

  g_permission_acquire_async(permission, g_task_get_cancellable(task),
                             permission_acquired_cb, task);
  g_steal_pointer(&task);
}

// Check if authorized to perform action with [action_id].
void ua_check_authorization(const gchar *action_id,
                            GDBusMethodInvocation *invocation,
                            GCancellable *cancellable,
                            GAsyncReadyCallback callback,
                            gpointer callback_data) {
  g_autoptr(GTask) task =
      g_task_new(NULL, cancellable, callback, callback_data);

  g_autoptr(PolkitSubject) subject = polkit_system_bus_name_new(
      g_dbus_method_invocation_get_sender(invocation));
  polkit_permission_new(action_id, subject, cancellable, permission_cb,
                        g_steal_pointer(&task));
}

// Complete request started with ua_check_authorization().
gboolean ua_check_authorization_finish(GAsyncResult *result, GError **error) {
  return g_task_propagate_boolean(G_TASK(result), error);
}
