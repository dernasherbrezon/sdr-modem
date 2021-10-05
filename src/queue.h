#ifndef QUEUE_H_
#define QUEUE_H_

#include <stdint.h>
#include <complex.h>
#include <stdbool.h>

typedef struct queue_t queue;

int create_queue(uint32_t buffer_size, uint16_t queue_size, bool blocking, queue **queue);

void queue_put(const float complex *buffer, const size_t len, queue *queue);
void take_buffer_for_processing(float complex **buffer, size_t *len, queue *queue);
void complete_buffer_processing(queue *queue);

void interrupt_waiting_the_data(queue *queue);
void destroy_queue(queue *queue);

#endif /* QUEUE_H_ */
