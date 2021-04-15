#include "linked_list.h"
#include <errno.h>
#include <stdlib.h>

struct linked_list_t {
    struct linked_list_t *next;
    void *data;
    void (*destructor)(void *data);
};

int linked_list_create(void *data, void (*destructor)(void *data), linked_list **list) {
    struct linked_list_t *result = malloc(sizeof(struct linked_list_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    *result = (struct linked_list_t) {0};

    result->data = data;
    result->destructor = destructor;

    *list = result;
    return 0;
}

int linked_list_add(void *data, void (*destructor)(void *), linked_list *list) {
    linked_list *node = list;
    while (node->next != NULL) {
        node = node->next;
    }
    return linked_list_create(data, destructor, &node->next);
}

void linked_list_destroy_by_selector(bool (*selector)(void *), linked_list **list) {
    linked_list *cur_node = *list;
    linked_list *previous = NULL;
    while (cur_node != NULL) {
        linked_list *next = cur_node->next;
        if (selector(next->data) == false) {
            previous = cur_node;
            cur_node = next;
            continue;
        }
        if (previous == NULL) {
            *list = next;
        } else {
            previous->next = next;
        }
        cur_node->destructor(cur_node->data);
        free(cur_node);
        cur_node = next;
    }
}

void linked_list_destroy(linked_list *list) {
    if (list == NULL) {
        return;
    }
    linked_list *cur = list;
    while (cur != NULL) {
        linked_list *next = cur->next;
        cur->destructor(cur->data);
        free(cur);
        cur = next;
    }
}

int linked_list_size(linked_list *list) {
    int result = 0;
    linked_list *cur = list;
    while (cur != NULL) {
        result++;
        cur = cur->next;
    }
    return result;
}