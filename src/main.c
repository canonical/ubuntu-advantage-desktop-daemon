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
  const GOptionEntry options[] = {{"replace", 'r', 0, G_OPTION_ARG_NONE,
                                   &replace, _("Replace current daemon"), NULL},
                                  {"version", 'v', 0, G_OPTION_ARG_NONE,
                                   &show_version, _("Show daemon version"),
                                   NULL},
                                  {NULL}};
  g_autoptr(GOptionContext) context = g_option_context_new("ua-daemon");
  g_option_context_add_main_entries(context, options, NULL);
  g_autoptr(GError) error = NULL;
  if (!g_option_context_parse(context, &argc, &argv, &error)) {
    g_printerr("Failed to parse command-line options: %s\n", error->message);
    return EXIT_FAILURE;
  }
  if (show_version) {
    g_printerr("ua-daemon %s\n", PROJECT_VERSION);
    return EXIT_SUCCESS;
  }

  g_autoptr(GMainLoop) loop = g_main_loop_new(NULL, FALSE);

  g_autoptr(UaDaemon) daemon = ua_daemon_new(replace);
  g_signal_connect(daemon, "quit", G_CALLBACK(quit_cb), loop);
  if (!ua_daemon_start(daemon, &error)) {
    g_printerr("Failed to start daemon: %s\n", error->message);
    return EXIT_FAILURE;
  }

  g_main_loop_run(loop);

  return EXIT_SUCCESS;
}
