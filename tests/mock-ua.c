#include <glib.h>
#include <json-glib/json-glib.h>

static JsonObject *get_status() {
  const gchar *status_file = getenv("MOCK_UA_STATUS_FILE");
  if (status_file == NULL) {
    return NULL;
  }

  g_autoptr(JsonParser) parser = json_parser_new();
  g_autoptr(GError) error = NULL;
  if (!json_parser_load_from_file(parser, status_file, &error)) {
    g_printerr("Failed to load status: %s\n", error->message);
    JsonObject *object = json_object_new();
    json_object_set_boolean_member(object, "attached", FALSE);
    return object;
  }

  JsonNode *root = json_parser_get_root(parser);
  JsonObject *status = json_node_get_object(root);
  return json_object_ref(status);
}

static void update_status(JsonObject *status) {
  g_autoptr(JsonGenerator) generator = json_generator_new();
  g_autoptr(JsonNode) root = json_node_new(JSON_NODE_OBJECT);
  json_node_set_object(root, status);
  json_generator_set_root(generator, root);
  g_autofree gchar *status_json = json_generator_to_data(generator, NULL);

  const gchar *status_file = getenv("MOCK_UA_STATUS_FILE");

  g_autoptr(GError) error = NULL;
  if (!g_file_set_contents(status_file, status_json, -1, &error)) {
    g_printerr("Failed to write status: %s\n", error->message);
  }
}

static int attach(int argc, char **argv) {
  if (argc != 1) {
    g_printerr("Invalid args\n");
    return EXIT_FAILURE;
  }
  const char *token = argv[0];

  g_autoptr(JsonObject) status = get_status();
  gboolean attached = json_object_get_boolean_member(status, "attached");

  if (attached) {
    g_printerr("Already attached\n");
    return EXIT_SUCCESS;
  }

  if (strcmp(token, "1234") != 0) {
    g_printerr("Invalid token\n");
    return EXIT_FAILURE;
  }

  json_object_set_boolean_member(status, "attached", TRUE);
  update_status(status);

  return EXIT_SUCCESS;
}

static int detach(int argc, char **argv) {
  if (argc != 1 || strcmp(argv[0], "--assume-yes") != 0) {
    g_printerr("Invalid args\n");
    return EXIT_FAILURE;
  }

  g_autoptr(JsonObject) status = get_status();
  gboolean attached = json_object_get_boolean_member(status, "attached");

  if (!attached) {
    g_printerr("Not attached\n");
    return EXIT_SUCCESS;
  }

  json_object_set_boolean_member(status, "attached", FALSE);
  update_status(status);

  return EXIT_SUCCESS;
}

static JsonObject *find_service(JsonObject *status, const char *name) {
  JsonArray *services = json_object_get_array_member(status, "services");
  for (guint i = 0; i < json_array_get_length(services); i++) {
    JsonObject *service = json_array_get_object_element(services, i);
    if (g_strcmp0(name, json_object_get_string_member(service, "name")) == 0) {
      return service;
    }
  }

  return NULL;
}

static int disable(int argc, char **argv) {
  if (argc != 2 || strcmp(argv[0], "--assume-yes") != 0) {
    g_printerr("Invalid args\n");
    return EXIT_FAILURE;
  }
  const char *name = argv[1];

  g_autoptr(JsonObject) status = get_status();
  JsonObject *service = find_service(status, name);
  if (service == NULL) {
    g_printerr("Unknown service\n");
    return EXIT_FAILURE;
  }

  json_object_set_string_member(service, "status", "disabled");
  update_status(status);

  return EXIT_SUCCESS;
}

static int enable(int argc, char **argv) {
  if (argc != 2 || strcmp(argv[0], "--assume-yes") != 0) {
    g_printerr("Invalid args\n");
    return EXIT_FAILURE;
  }
  const char *name = argv[1];

  g_autoptr(JsonObject) status = get_status();
  JsonObject *service = find_service(status, name);
  if (service == NULL) {
    g_printerr("Unknown service\n");
    return EXIT_FAILURE;
  }

  json_object_set_string_member(service, "status", "enabled");
  update_status(status);

  return EXIT_SUCCESS;
}

static int usage() {
  g_printerr("Usage: ua <command> [flags]\n");
  return EXIT_FAILURE;
}

int main(int argc, char **argv) {
  const char *command = "";
  int command_argc = 0;
  char **command_argv = NULL;
  if (argc >= 2) {
    command = argv[1];
    command_argc = argc - 2;
    command_argv = argv + 2;
  }
  if (g_strcmp0(command, "attach") == 0) {
    return attach(command_argc, command_argv);
  } else if (g_strcmp0(command, "detach") == 0) {
    return detach(command_argc, command_argv);
  } else if (g_strcmp0(command, "disable") == 0) {
    return disable(command_argc, command_argv);
  } else if (g_strcmp0(command, "enable") == 0) {
    return enable(command_argc, command_argv);
  } else {
    return usage();
  }
}
