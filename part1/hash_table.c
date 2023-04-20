#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hash_table.h"

list *newlist() {
    list *list = malloc(sizeof(list));
    list->next = NULL;
    return list;
}

void push_back(list *head, void *data) {
    list *ptr = head, *new_node = malloc(sizeof(list));
    while (ptr->next != NULL) ptr = ptr->next;
    new_node->data = data;
    new_node->next = NULL;
    ptr->next = new_node;
}

hashtable *newtable(int bucket) {
    hashtable *table = malloc(sizeof(hashtable));
    table->bucket = bucket;
    table->buckets = malloc(sizeof(list*) * bucket);
    for (int i = 0; i < bucket; i++)
        table->buckets[i] = newlist();
    return table;
}

void add(hashtable *table, void *data, int key_index, int size) {
    int key = *((int*)data + key_index);
    int hash = key % table->bucket;
    int *copy = malloc(size);
    memcpy(copy, data, size);
    push_back(table->buckets[hash], copy);
}

void print_items(list *head) {
    list *ptr = head->next;
    printf("[");
    while (ptr != NULL) {
        printf("%x", ptr->data);
        if (ptr->next != NULL) printf(", ");
        ptr = ptr->next;
    }
    printf("]\n");
}