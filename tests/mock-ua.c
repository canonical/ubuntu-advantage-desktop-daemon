#include <glib.h>
#include <json-glib/json-glib.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

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
  g_autoptr(GError) error = NULL;
  g_autofree char *config_file = NULL;
  g_autofree char *format = NULL;
  gboolean no_auto_enable = FALSE;

  const GOptionEntry options_entries[] = {
      {"no-auto-enable", 0, 0, G_OPTION_ARG_NONE, &no_auto_enable, NULL, NULL},
      {"attach-config", 0, 0, G_OPTION_ARG_FILENAME, &config_file, NULL, NULL},
      {"format", 0, 0, G_OPTION_ARG_STRING, &format, NULL, NULL},
      {NULL},
  };

  g_autoptr(GOptionContext) options_context =
      g_option_context_new("<token> [flags]");
  g_option_context_add_main_entries(options_context, options_entries, NULL);

  if (!g_option_context_parse(options_context, &argc, &argv, &error)) {
    g_critical("Options parse error: %s", error->message);
    return EXIT_FAILURE;
  }

  g_autofree char *token = NULL;
  g_assert_cmpint(argc, >, 1);
  if (argc > 2)
    token = g_strdup(argv[2]);

  if (config_file) {
    g_autofree char *config_file_contents = NULL;
    g_autoptr(GRegex) yaml_regex = NULL;
    g_autoptr(GMatchInfo) info = NULL;

    if (!g_file_get_contents(config_file, &config_file_contents, NULL,
                             &error)) {
      g_critical("Cannot open config file %s: %s", config_file, error->message);
      return EXIT_FAILURE;
    }

    // Ensure the file is only readable by the current user.
    struct stat statbuf;
    g_assert_cmpint(stat(config_file, &statbuf), ==, 0);
    g_assert_cmpint(statbuf.st_mode, ==, S_IFREG | 0600);

    yaml_regex = g_regex_new("^token:\\s*(\\w+)$",
                             G_REGEX_OPTIMIZE | G_REGEX_MULTILINE, 0, NULL);

    if (!g_regex_match(yaml_regex, config_file_contents, 0, &info)) {
      g_critical("Can't find token in config file %s", config_file);
      return EXIT_FAILURE;
    }

    g_free(token);
    token = g_match_info_fetch(info, g_match_info_get_match_count(info) - 1);
  }

  g_printerr("CLI: Received token is '%s'\n", token);

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
    return attach(argc, argv);
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
