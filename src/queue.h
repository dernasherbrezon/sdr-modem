#ifndef QUEUE_H_
#define QUEUE_H_

#include <stdint.h>
#include <complex.h>
#include <stdbool.h>

typedef struct queue_t queue;

typedef struct {
  uint8_t type;
  uint32_t request_id;
  void *buffer;
  size_t buffer_len;
} queue_message;

int create_queue(uint32_t buffer_size, uint16_t queue_size, bool blocking, queue **queue);

int queue_put(const queue_message *message, queue *queue);

void queue_take(queue_message *message, queue *queue);

void queue_complete(queue *queue);

void interrupt_waiting_the_data(queue *queue);

void destroy_queue(queue *queue);

#endif /* QUEUE_H_ */
