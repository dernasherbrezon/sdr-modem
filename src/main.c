#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <getopt.h>

#include "app_config.h"
#include "tcp_server.h"

static tcp_server *server = NULL;

void sdrmodem_stop_async(int signum) {
  tcp_server_destroy(server);
  server = NULL;
}

int main(int argc, char **argv) {
  static struct option long_options[] = {
    {"config", required_argument, NULL, 'c'},
    {"input", required_argument, NULL, 'i'},
    {"output", required_argument, NULL, 'o'},
    {NULL, 0, NULL, 0}
  };

  const char *config_path = NULL;
  const char *input_path = NULL;
  const char *output_path = NULL;
  int opt;
  while ((opt = getopt_long(argc, argv, "c:i:o:", long_options, NULL)) != -1) {
    switch (opt) {
      case 'c':
        config_path = optarg;
        break;
      case 'i':
        input_path = optarg;
        break;
      case 'o':
        output_path = optarg;
        break;
      default:
        fprintf(stderr, "usage: %s\n -c|--config <configuration file>\n -i|--input <input file>\n -o|--output <output file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
  }
  if (config_path == NULL) {
    fprintf(stderr, "<3>parameter missing: -c/--config <configuration file>\n");
    exit(EXIT_FAILURE);
  }
  setvbuf(stdout, NULL, _IOLBF, 0);

  app_config *app_config = NULL;
  int code = app_config_create(config_path, &app_config);
  if (code != 0) {
    exit(EXIT_FAILURE);
  }
  if (app_config->bind_address != NULL && (input_path != NULL || output_path != NULL)) {
    fprintf(stderr, "<3>bind_address (server mode) is configured, while running with the CLI arguments\n");
    exit(EXIT_FAILURE);
  }
  if (app_config->bind_address == NULL && (input_path == NULL || output_path == NULL)) {
    fprintf(stderr, "<3>bind_address (server mode) is not configured and input/output file configuration is missing\n");
    exit(EXIT_FAILURE);
  }

  signal(SIGINT, sdrmodem_stop_async);
  signal(SIGHUP, sdrmodem_stop_async);
  signal(SIGTERM, sdrmodem_stop_async);
  signal(SIGPIPE, SIG_IGN);

  code = tcp_server_create(app_config, &server);
  if (code != 0) {
    app_config_destroy(app_config);
    exit(EXIT_FAILURE);
  }

  // wait here until server terminates
  tcp_server_join_thread(server);

  // server will be freed on its own thread
  app_config_destroy(app_config);
}
