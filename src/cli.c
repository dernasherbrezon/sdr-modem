#include "cli.h"
#include <stdlib.h>

struct cli_t {
};

int cli_create(app_config *config, cli **result) {
  return 0;
}

int cli_process(int argc, char **argv, cli *cli) {
  return 0;
}

void cli_destroy(cli *cli) {
  if (cli == NULL) {
    return;
  }
}
