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
      {NULL, 0, NULL, 0}};

  const char *config_path = NULL;
  int opt;
  while ((opt = getopt_long(argc, argv, "c:", long_options, NULL)) != -1) {
    switch (opt) {
      case 'c':
        config_path = optarg;
        break;
      default:
        fprintf(stderr, "usage: %s -c|--config <configuration file>\n", argv[0]);
        exit(EXIT_FAILURE);
    }
  }
  if (config_path == NULL) {
    fprintf(stderr, "<3>parameter missing: -c/--config <configuration file>\n");
    exit(EXIT_FAILURE);
  }
  setvbuf(stdout, NULL, _IOLBF, 0);

  app_config *server_config = NULL;
  int code = app_config_create(config_path, &server_config);
  if (code != 0) {
    exit(EXIT_FAILURE);
  }

  signal(SIGINT, sdrmodem_stop_async);
  signal(SIGHUP, sdrmodem_stop_async);
  signal(SIGTERM, sdrmodem_stop_async);
  signal(SIGPIPE, SIG_IGN);

  code = tcp_server_create(server_config, &server);
  if (code != 0) {
    app_config_destroy(server_config);
    exit(EXIT_FAILURE);
  }

  // wait here until server terminates
  tcp_server_join_thread(server);

  // server will be freed on its own thread
  app_config_destroy(server_config);
}
