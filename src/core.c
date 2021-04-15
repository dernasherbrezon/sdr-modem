#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <stdbool.h>
#include <pthread.h>
#include <string.h>
#include <complex.h>
#include <unistd.h>

#include "core.h"
#include "api.h"

struct linked_list_node {
	struct linked_list_node *next;
	struct client_config *config;
	queue *queue;
	pthread_t dsp_thread;
	FILE *file;
};

struct core_t {
	struct server_config *server_config;
	pthread_mutex_t mutex;
	pthread_cond_t sdr_thread_stopped_condition;

	struct linked_list_node *client_configs;
	pthread_t sdr_worker_thread;
	uint8_t *buffer;
};

int core_create(struct server_config *server_config, core **result) {
	core *core = malloc(sizeof(struct core_t));
	if (core == NULL) {
		return -ENOMEM;
	}
	// init all fields with 0 so that destroy_* method would work
	*core = (struct core_t ) { 0 };

	core->server_config = server_config;
	uint8_t *buffer = malloc(server_config->buffer_size * sizeof(uint8_t));
	if (buffer == NULL) {
        core_destroy(core);
		return -ENOMEM;
	}
	memset(buffer, 0, server_config->buffer_size);
	core->buffer = buffer;
	core->mutex = (pthread_mutex_t )PTHREAD_MUTEX_INITIALIZER;
	core->sdr_thread_stopped_condition = (pthread_cond_t )PTHREAD_COND_INITIALIZER;
	*result = core;
	return 0;
}

int write_to_file(struct linked_list_node *config_node, float complex *filter_output, size_t filter_output_len) {
	size_t n_written;
	if (config_node->file != NULL) {
		n_written = fwrite(filter_output, sizeof(float complex), filter_output_len, config_node->file);
	} else {
		fprintf(stderr, "<3>unknown file output\n");
		return -1;
	}
	// if disk is full, then terminate the client
	if (n_written < filter_output_len) {
		return -1;
	} else {
		return 0;
	}
}

int write_to_socket(struct linked_list_node *config_node, float complex *filter_output, size_t filter_output_len) {
	size_t total_len = filter_output_len * sizeof(float complex);
	size_t left = total_len;
	while (left > 0) {
		int written = write(config_node->config->client_socket, (char*) filter_output + (total_len - left), left);
		if (written < 0) {
			return -1;
		}
		left -= written;
	}
	return 0;
}

static void* dsp_worker(void *arg) {
	struct linked_list_node *config_node = (struct linked_list_node*) arg;
	fprintf(stdout, "[%d] dsp_worker is starting\n", config_node->config->id);
	uint8_t *input = NULL;
	int input_len = 0;
	float complex *filter_output = NULL;
	size_t filter_output_len = 0;
	while (true) {
//		take_buffer_for_processing(&input, &input_len, config_node->queue);
		// poison pill received
		if (input == NULL) {
			break;
		}
//		process(input, input_len, &filter_output, &filter_output_len, config_node->filter);
		int code;
//		if (config_node->config->destination == REQUEST_RX_DESTINATION_FILE) {
//			code = write_to_file(config_node, filter_output, filter_output_len);
//		} else if (config_node->config->destination == REQUEST_RX_DESTINATION_SOCKET) {
//			code = write_to_socket(config_node, filter_output, filter_output_len);
//		} else {
//			fprintf(stderr, "<3>unknown destination: %d\n", config_node->config->destination);
//			code = -1;
//		}

//		complete_buffer_processing(config_node->queue);

		if (code != 0) {
			// this would trigger client disconnect
			// I could use "break" here, but "continue" is a bit better:
			//   - single route for abnormal termination (i.e. disk space issue) and normal (i.e. client disconnected)
			//   - all shutdown sequences have: stop tcp thread, then dsp thread, then rtlsdr thread
			//   - processing the queue and writing to the already full disk is OK (I hope)
			//   - calling "close" socket multiple times is OK (I hope)
			close(config_node->config->client_socket);
		}

	}
//	destroy_queue(config_node->queue);
	printf("[%d] dsp_worker stopped\n", config_node->config->id);
	return (void*) 0;
}

static void* sdr_worker(void *arg) {
	core *core = (struct core_t*) arg;
	uint32_t buffer_size = core->server_config->buffer_size;
	//FIXME
	return (void*) 0;
}

int start_sdr(struct client_config *config) {
	core *core = config->core;
	fprintf(stdout, "sdr is starting\n");

	pthread_t sdr_worker_thread;
	int code = pthread_create(&sdr_worker_thread, NULL, &sdr_worker, core);
	if (code != 0) {
		return 0x04;
	}
	core->sdr_worker_thread = sdr_worker_thread;
	return 0x0;
}

void stop_sdr(core *core) {
	fprintf(stdout, "sdr is stopping\n");

	// release the mutex here for rtlsdr thread to send last updates
	// and cleans up
//	while (core->dev != NULL) {
//		pthread_cond_wait(&core->sdr_thread_stopped_condition, &core->mutex);
//	}

	// additionally wait until sdr thread terminates by OS
	pthread_join(core->sdr_worker_thread, NULL);
}

void destroy_node(struct linked_list_node *node) {
	if (node == NULL) {
		return;
	}
	fprintf(stdout, "[%d] dsp_worker is stopping\n", node->config->id);
//	if (node->queue != NULL) {
//		interrupt_waiting_the_data(node->queue);
//	}
	// wait until thread terminates and only then destroy the node
	pthread_join(node->dsp_thread, NULL);
	// cleanup everything only when thread terminates
	if (node->file != NULL) {
		fclose(node->file);
	}
//	if (node->filter != NULL) {
//		destroy_xlating(node->filter);
//	}
	free(node);
}

int core_add_client(struct client_config *config) {
	if (config == NULL) {
		return -1;
	}
	struct linked_list_node *config_node = malloc(sizeof(struct linked_list_node));
	if (config_node == NULL) {
		return -ENOMEM;
	}
	// init all fields with 0 so that destroy_* method would work
	*config_node = (struct linked_list_node ) { 0 };
	config_node->config = config;

	//FIXME setup demodulator


    char file_path[4096];
    snprintf(file_path, sizeof(file_path), "%s/%d.cf32", config->core->server_config->base_path, config->id);
    config_node->file = fopen(file_path, "wb");
    if (config_node->file == NULL) {
        fprintf(stderr, "<3>unable to open file for output: %s\n", file_path);
        destroy_node(config_node);
        return -1;
    }

	// setup queue
//	queue *client_queue = NULL;
//	code = create_queue(config->core->server_config->buffer_size, config->core->server_config->queue_size, &client_queue);
//	if (code != 0) {
//		destroy_node(config_node);
//		return -1;
//	}
//	config_node->queue = client_queue;

	// start processing
	pthread_t dsp_thread;
	int code = pthread_create(&dsp_thread, NULL, &dsp_worker, config_node);
	if (code != 0) {
		destroy_node(config_node);
		return -1;
	}
	config_node->dsp_thread = dsp_thread;

	int result;
	pthread_mutex_lock(&config->core->mutex);
	if (config->core->client_configs == NULL) {
		// init rtl-sdr only for the first client
		result = start_sdr(config);
		if (result == 0) {
			config->core->client_configs = config_node;
		}
	} else {
		struct linked_list_node *last_config = config->core->client_configs;
		while (last_config->next != NULL) {
			last_config = last_config->next;
		}
		last_config->next = config_node;
		result = 0;
	}
	pthread_mutex_unlock(&config->core->mutex);

	if (result != 0) {
		destroy_node(config_node);
	}
	return result;
}

void core_remove_client(struct client_config *config) {
	if (config == NULL) {
		return;
	}
	struct linked_list_node *node_to_destroy = NULL;
	bool should_stop_sdr = false;
	pthread_mutex_lock(&config->core->mutex);
	struct linked_list_node *cur_node = config->core->client_configs;
	struct linked_list_node *previous_node = NULL;
	while (cur_node != NULL) {
		if (cur_node->config->id == config->id) {
			if (previous_node == NULL) {
				if (cur_node->next == NULL) {
					// this is the first and the last node
					// shutdown sdr
                    should_stop_sdr = true;
				}
				// update pointer to the first node
				config->core->client_configs = cur_node->next;
			} else {
				previous_node->next = cur_node->next;
			}
			node_to_destroy = cur_node;
			break;
		}
		previous_node = cur_node;
		cur_node = cur_node->next;
	}
	if (should_stop_sdr) {
		stop_sdr(config->core);
	}
	pthread_mutex_unlock(&config->core->mutex);
	// stopping the thread can take some time
	// do it outside of synch block
	if (node_to_destroy != NULL) {
		destroy_node(node_to_destroy);
	}
}

void core_destroy(core *core) {
	if (core == NULL) {
		return;
	}
	pthread_mutex_lock(&core->mutex);
//	if (core->dev != NULL) {
//		stop_rtlsdr(core);
//	}
	if (core->buffer != NULL) {
		free(core->buffer);
	}
	struct linked_list_node *cur_node = core->client_configs;
	while (cur_node != NULL) {
		struct linked_list_node *next = cur_node->next;
		// destroy all nodes in synch block, because
		// destory_core should be atomic operation for external code,
		// i.e. destroy all client_configs
		destroy_node(cur_node);
		cur_node = next;
	}
	core->client_configs = NULL;
	pthread_mutex_unlock(&core->mutex);
	free(core);
}

