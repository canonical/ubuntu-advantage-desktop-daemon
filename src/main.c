#include <glib/gi18n.h>
#include <locale.h>

#include "config.h"
#include "ua-daemon.h"

static void quit_cb(UaDaemon *daemon, GMainLoop *loop) {
  g_main_loop_quit(loop);
}

int main(int argc, char **argv) {
  setlocale(LC_ALL, "");
  bind_textdomain_codeset(GETTEXT_PACKAGE, "UTF-8");

  gboolean replace = FALSE;
  gboolean show_version = FALSE;
  g_autofree gchar *status_path = NULL;
  const GOptionEntry options[] = {
      {"replace", 'r', 0, G_OPTION_ARG_NONE, &replace,
       _("Replace current daemon"), NULL},
      {"status-path", 0, 0, G_OPTION_ARG_STRING, &status_path,
       _("Path to status file"), "PATH"},
      {"version", 'v', 0, G_OPTION_ARG_NONE, &show_version,
       _("Show daemon version"), NULL},
      {NULL}};
  g_autoptr(GOptionContext) context =
      g_option_context_new("ubuntu-advantage-desktop-daemon");
  g_option_context_add_main_entries(context, options, NULL);
  g_autoptr(GError) error = NULL;
  if (!g_option_context_parse(context, &argc, &argv, &error)) {
    g_printerr("Failed to parse command-line options: %s\n", error->message);
    return EXIT_FAILURE;
  }
  if (show_version) {
    g_printerr("ubuntu-advantage-desktop-daemon %s\n", PROJECT_VERSION);
    return EXIT_SUCCESS;
  }

  if (status_path == NULL) {
    status_path = g_strdup("/var/lib/ubuntu-advantage/status.json");
  }

  g_autoptr(GMainLoop) loop = g_main_loop_new(NULL, FALSE);

  g_autoptr(UaDaemon) daemon = ua_daemon_new(replace, status_path);
  g_signal_connect(daemon, "quit", G_CALLBACK(quit_cb), loop);
  if (!ua_daemon_start(daemon, &error)) {
    g_printerr("Failed to start daemon: %s\n", error->message);
    return EXIT_FAILURE;
  }

  g_main_loop_run(loop);

  return EXIT_SUCCESS;
}
