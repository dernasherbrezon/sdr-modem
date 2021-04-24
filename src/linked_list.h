#ifndef SDR_MODEM_LINKED_LIST_H
#define SDR_MODEM_LINKED_LIST_H

#include <stdbool.h>

typedef struct linked_list_t linked_list;

int linked_list_add(void *data, void (*destructor)(void *data), linked_list **list);

void linked_list_destroy_by_selector(bool (*selector)(void *data), linked_list **list);

void linked_list_foreach(void *arg, void (*foreach)(void *arg, void *data), linked_list *list);

void linked_list_destroy_by_id(void *id, bool (*selector)(void *id, void *data), linked_list **list);

void *linked_list_find(void *id, bool (*selector)(void *id, void *data), linked_list *list);

void linked_list_destroy(linked_list *list);

#endif //SDR_MODEM_LINKED_LIST_H
