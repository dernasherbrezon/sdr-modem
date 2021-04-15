#ifndef SDR_MODEM_LINKED_LIST_H
#define SDR_MODEM_LINKED_LIST_H

#include <stdbool.h>

typedef struct linked_list_t linked_list;

int linked_list_create(void *data, void (*destructor)(void *), linked_list **list);
int linked_list_add(void *data, void (*destructor)(void *), linked_list *list);
void linked_list_destroy_by_selector(bool (*selector)(void *), linked_list **list);
int linked_list_size(linked_list *list);

void linked_list_destroy(linked_list *list);

#endif //SDR_MODEM_LINKED_LIST_H
