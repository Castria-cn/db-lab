#include "set.h"
#include <stdlib.h>
#include <string.h>

set* newset(char *opt_name) {
    set *ret = malloc(sizeof(set));
    ret->next = NULL;
    ret->opt_name = opt_name;
    return ret;
}

void insert(set *S, void *item) {
    set *ptr = S;
    while (ptr->next) ptr = ptr->next;
    set *newnode = malloc(sizeof(set));
    newnode->next = NULL;
    newnode->item = item;
    ptr->next = newnode;
}

set* join(set *S, set *T) {
    set *_S = copy(S), *_T = copy(T);
    set *ptr = _S;
    while (ptr->next) ptr = ptr->next;
    ptr->next = _T->next;
    return _S;
}

set* copy(set *S) {
    if (!S) return NULL;
    set *head = malloc(sizeof(set));
    head->item = S->item;
    head->opt_name = S->opt_name;
    head->next = copy(S->next);
    return head;
}

int has_str(set *S, void *item) {
    set *ptr = S->next;
    while (ptr) {
        if (strcmp(ptr->item, item) == 0) return 1;
        ptr = ptr->next;
    }
    return 0;
}

set* find_by_name(set *S, char *name) {
    set *ptr = S->next;
    while (ptr) {
        if (strcmp(((set*)(ptr->item))->opt_name, name) == 0) return ptr->item;
        ptr = ptr->next;
    }
    return NULL;
}

set* intersect(set *S, set *T) {
    set *_S = copy(S), *ptr = _S;
    while (ptr->next) {
        set *nxt = ptr->next;
        if (!has_str(T, nxt->item)) {
            ptr->next = ptr->next->next; //remove next item
        }
        else {
            ptr = ptr->next;
        }
    }
    return _S;
}

set* unique(set *S) {
    set *ptr = S;
    while (ptr->next) {
        if (has_str(ptr, ptr->next->item)) ptr->next = ptr->next->next;
        ptr = ptr->next; 
    }
    return S;
}
