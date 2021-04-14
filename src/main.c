#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "server_config.h"
#include "tcp_server.h"
#include "core.h"

static tcp_server *server = NULL;

void sdrmodem_stop_async(int signum) {
	tcp_server_destroy(server);
	server = NULL;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "<3>parameter missing: configuration file\n");
		exit(EXIT_FAILURE);
	}
	setvbuf(stdout, NULL, _IOLBF, 0);
	printf("hello world\n");
	struct server_config *server_config = NULL;
	int code = server_config_create(&server_config, argv[1]);
	if (code != 0) {
		exit(EXIT_FAILURE);
	}

	core *core = NULL;
	code = core_create(server_config, &core);
	if (code != 0) {
		server_config_destroy(server_config);
		exit(EXIT_FAILURE);
	}

	signal(SIGINT, sdrmodem_stop_async);
	signal(SIGHUP, sdrmodem_stop_async);
	signal(SIGTERM, sdrmodem_stop_async);

	code = tcp_server_create(server_config, core, &server);
	if (code != 0) {
        core_destroy(core);
		server_config_destroy(server_config);
		exit(EXIT_FAILURE);
	}

	// wait here until server terminates
	tcp_server_join_thread(server);

	// server will be freed on its own thread
    core_destroy(core);
	server_config_destroy(server_config);
}

