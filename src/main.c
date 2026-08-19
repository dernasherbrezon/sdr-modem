#include <stdio.h>
#include <signal.h>
#include <stdlib.h>
#include <getopt.h>

#include "app_config.h"
#include "tcp_server.h"
#include "cli.h"

static tcp_server *server = NULL;
static cli *cli_runner = NULL;

void sdrmodem_stop_async(int signum) {
  tcp_server_destroy(server);
  cli_destroy(cli_runner);
  server = NULL;
}

int main(int argc, char **argv) {
  setvbuf(stdout, NULL, _IOLBF, 0);
  app_config *app_config = NULL;
  int code = app_config_create(argc, argv, &app_config);
  if (code != 0) {
    exit(EXIT_FAILURE);
  }

  signal(SIGINT, sdrmodem_stop_async);
  signal(SIGHUP, sdrmodem_stop_async);
  signal(SIGTERM, sdrmodem_stop_async);
  signal(SIGPIPE, SIG_IGN);

  if (app_config->bind_address == NULL) {
    fprintf(stdout, "starting in the cli mode\n");
    code = cli_create(app_config, &cli_runner);
    if (code != 0) {
      app_config_destroy(app_config);
      exit(EXIT_FAILURE);
    }
    return cli_process(argc, argv, cli_runner);
  }

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
