
#ifndef CORE_H_
#define CORE_H_

#include <stdint.h>
#include <signal.h>
#include <stdatomic.h>
#include "queue.h"
#include "server_config.h"
#include "api.h"

typedef struct core_t core;

struct client_config {

    struct request *req;

	int client_socket;
	uint8_t id;
	atomic_bool is_running;
	core *core;

    queue *queue;
    pthread_t dsp_thread;
    FILE *file;
};

int core_create(struct server_config *server_config, core **result);

// client_config contains core to modify
int core_add_client(struct client_config *config);
void core_remove_client(struct client_config *config);

void core_destroy(core *core);


#endif /* CORE_H_ */
