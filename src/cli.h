#ifndef SDR_MODEM_CLI_H
#define SDR_MODEM_CLI_H

#include "app_config.h"

typedef struct cli_t cli;

int cli_create(app_config *config, cli **result);

int cli_process(cli *cli);

void cli_stop(cli *cli);

void cli_destroy(cli *cli);

#endif //SDR_MODEM_CLI_H
