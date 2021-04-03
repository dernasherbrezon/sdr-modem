#include <stdio.h>
#include <stdlib.h>
#include <signal.h>

#include "config.h"
//#include "tcp_server.h"
//#include "core.h"
//
//static tcp_server *server = NULL;

void sdrmodem_stop_async(int signum) {
//	stop_tcp_server(server);
//	server = NULL;
}

int main(int argc, char **argv) {
	if (argc < 2) {
		fprintf(stderr, "<3>parameter missing: configuration file\n");
		exit(EXIT_FAILURE);
	}
	setvbuf(stdout, NULL, _IOLBF, 0);
	printf("hello world\n");
	struct server_config *server_config = NULL;
	int code = create_server_config(&server_config, argv[1]);
	if (code != 0) {
		exit(EXIT_FAILURE);
	}

//	core *core = NULL;
//	code = create_core(server_config, &core);
//	if (code != 0) {
//		destroy_server_config(server_config);
//		exit(EXIT_FAILURE);
//	}

	signal(SIGINT, sdrmodem_stop_async);
	signal(SIGHUP, sdrmodem_stop_async);
	signal(SIGTERM, sdrmodem_stop_async);

//	code = start_tcp_server(server_config, core, &server);
//	if (code != 0) {
//		destroy_core(core);
//		destroy_server_config(server_config);
//		exit(EXIT_FAILURE);
//	}
//
//	// wait here until server terminates
//	join_tcp_server_thread(server);
//
//	// server will be freed on its own thread
//	destroy_core(core);
//	destroy_server_config(server_config);
}

