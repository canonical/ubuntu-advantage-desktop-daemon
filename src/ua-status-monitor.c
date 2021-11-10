#include <gio/gio.h>
#include <json-glib/json-glib.h>

#include "config.h"
#include "ua-service.h"
#include "ua-status-monitor.h"

struct _UaStatusMonitor {
  GObject parent_instance;

  GFile *status_file;
  GFileMonitor *status_file_monitor;
  GCancellable *file_cancellable;

  UaStatus *status;
};

G_DEFINE_TYPE(UaStatusMonitor, ua_status_monitor, G_TYPE_OBJECT)

enum { SIGNAL_CHANGED, SIGNAL_LAST };

static guint signals[SIGNAL_LAST] = {0};

// Called when JSON parsing is complete.
static void ua_status_parse_cb(GObject *object, GAsyncResult *result,
                               gpointer user_data) {
  UaStatusMonitor *self = user_data;
  JsonParser *parser = JSON_PARSER(object);

  g_autoptr(GError) error = NULL;
  if (!json_parser_load_from_stream_finish(parser, result, &error)) {
    g_warning("Failed to parse UA status: %s", error->message);
    return;
  }
  JsonNode *root = json_parser_get_root(parser);
  if (!JSON_NODE_HOLDS_OBJECT(root)) {
    g_warning("Invalid UA status JSON");
    return;
  }
  JsonObject *status = json_node_get_object(root);

  gboolean attached = json_object_get_boolean_member(status, "attached");

  g_autoptr(GPtrArray) services =
      g_ptr_array_new_with_free_func(g_object_unref);
  JsonArray *services_array = json_object_get_array_member(status, "services");
  for (guint i = 0; i < json_array_get_length(services_array); i++) {
    JsonObject *s = json_array_get_object_element(services_array, i);
    g_ptr_array_add(services,
                    ua_service_new(json_object_get_string_member(s, "name"),
                                   json_object_get_string_member(s, "entitled"),
                                   json_object_get_string_member(s, "status")));
  }

  g_clear_object(&self->status);
  self->status = ua_status_new(attached, services);

  g_signal_emit(self, signals[SIGNAL_CHANGED], 0);
}

static void parse_status_file(UaStatusMonitor *self) {
  if (self->file_cancellable != NULL) {
    g_cancellable_cancel(self->file_cancellable);
  }
  g_clear_object(&self->file_cancellable);
  self->file_cancellable = g_cancellable_new();

  g_autoptr(GError) error = NULL;
  g_autoptr(GFileInputStream) stream =
      g_file_read(self->status_file, self->file_cancellable, &error);
  if (stream == NULL) {
    if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND)) {
      g_clear_object(&self->status);
      g_autoptr(GPtrArray) services =
          g_ptr_array_new_with_free_func(g_object_unref);
      self->status = ua_status_new(FALSE, services);
      g_signal_emit(self, signals[SIGNAL_CHANGED], 0);
    } else {
      g_printerr("Failed to read UA status file: %s\n", error->message);
    }
    return;
  }

  g_autoptr(JsonParser) parser = json_parser_new();
  json_parser_load_from_stream_async(parser, G_INPUT_STREAM(stream),
                                     self->file_cancellable, ua_status_parse_cb,
                                     self);
}

static void status_file_changed_cb(UaStatusMonitor *self, GFile *file,
                                   GFile *other_file,
                                   GFileMonitorEvent event_type) {
  if (event_type == G_FILE_MONITOR_EVENT_CHANGED ||
      event_type == G_FILE_MONITOR_EVENT_DELETED) {
    parse_status_file(self);
  }
}

static void ua_status_monitor_dispose(GObject *object) {
  UaStatusMonitor *self = UA_STATUS_MONITOR(object);

  if (self->file_cancellable != NULL) {
    g_cancellable_cancel(self->file_cancellable);
  }

  g_clear_object(&self->status_file);
  g_clear_object(&self->status_file_monitor);
  g_clear_object(&self->status);
  g_clear_object(&self->file_cancellable);

  G_OBJECT_CLASS(ua_status_monitor_parent_class)->dispose(object);
}

static void ua_status_monitor_init(UaStatusMonitor *self) {
  self->status_file =
      g_file_new_for_path("/var/lib/ubuntu-advantage/status.json");
}

static void ua_status_monitor_class_init(UaStatusMonitorClass *klass) {
  G_OBJECT_CLASS(klass)->dispose = ua_status_monitor_dispose;

  signals[SIGNAL_CHANGED] =
      g_signal_new("changed", G_TYPE_FROM_CLASS(G_OBJECT_CLASS(klass)),
                   G_SIGNAL_RUN_LAST, 0, NULL, NULL, NULL, G_TYPE_NONE, 0);
}

UaStatusMonitor *ua_status_monitor_new(gboolean replace) {
  return (UaStatusMonitor *)g_object_new(ua_status_monitor_get_type(), NULL);
}

gboolean ua_status_monitor_start(UaStatusMonitor *self, GError **error) {
  g_return_val_if_fail(UA_IS_STATUS_MONITOR(self), FALSE);
  self->status_file_monitor =
      g_file_monitor_file(self->status_file, G_FILE_MONITOR_NONE, NULL, error);
  if (self->status_file_monitor == NULL) {
    return FALSE;
  } else {
    g_signal_connect_swapped(self->status_file_monitor, "changed",
                             G_CALLBACK(status_file_changed_cb), self);
  }

  // Read initial status.
  parse_status_file(self);

  return TRUE;
}

UaStatus *ua_status_monitor_get_status(UaStatusMonitor *self) {
  g_return_val_if_fail(UA_IS_STATUS_MONITOR(self), NULL);
  return self->status;
}
