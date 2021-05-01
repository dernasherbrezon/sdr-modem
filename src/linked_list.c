#include "linked_list.h"
#include <errno.h>
#include <stdlib.h>

struct linked_list_t {
    struct linked_list_t *next;

    void *data;

    void (*destructor)(void *data);
};

int linked_list_add(void *data, void (*destructor)(void *data), linked_list **list) {
    struct linked_list_t *result = malloc(sizeof(struct linked_list_t));
    if (result == NULL) {
        return -ENOMEM;
    }
    *result = (struct linked_list_t) {0};

    result->data = data;
    result->destructor = destructor;

    if (*list == NULL) {
        *list = result;
    } else {
        linked_list *node = *list;
        while (node->next != NULL) {
            node = node->next;
        }
        node->next = result;
    }
    return 0;
}

void linked_list_foreach(void *arg, void (*foreach)(void *arg, void *data), linked_list *list) {
    linked_list *cur = list;
    while (cur != NULL) {
        foreach(arg, cur->data);
        cur = cur->next;
    }
}

void *linked_list_find(void *id, bool (*selector)(void *id, void *data), linked_list *list) {
    linked_list *cur_node = list;
    while (cur_node != NULL) {
        if (selector(id, cur_node->data) == true) {
            return cur_node->data;
        }
        cur_node = cur_node->next;
    }
    return NULL;
}

void *linked_list_remove_by_id(void *id, bool (*selector)(void *id, void *data), linked_list **list) {
    linked_list *cur_node = *list;
    linked_list *previous = NULL;
    void *result = NULL;
    while (cur_node != NULL) {
        linked_list *next = cur_node->next;
        if (selector(id, cur_node->data) == false) {
            previous = cur_node;
            cur_node = next;
            continue;
        }
        if (previous == NULL) {
            *list = next;
        } else {
            previous->next = next;
        }
        result = cur_node->data;
        free(cur_node);
        break;
    }
    return result;
}

void linked_list_destroy_by_id(void *id, bool (*selector)(void *id, void *data), linked_list **list) {
    linked_list *cur_node = *list;
    linked_list *previous = NULL;
    while (cur_node != NULL) {
        linked_list *next = cur_node->next;
        if (selector(id, cur_node->data) == false) {
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
        break;
    }
}

void linked_list_destroy_by_selector(bool (*selector)(void *data), linked_list **list) {
    linked_list *cur_node = *list;
    linked_list *previous = NULL;
    while (cur_node != NULL) {
        linked_list *next = cur_node->next;
        if (selector(cur_node->data) == false) {
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
