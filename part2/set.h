typedef struct set {
    struct set *next;
    void *item;
    char *opt_name;
} set;

set* newset(char *opt_name);

void insert(set *S, void *item);

//ignore duplicate elements in list.
set* join(set *S, set *T);

set* copy(set *S);

int has_str(set *S, void *item);

set* find_by_name(set *S, char *name);

set* intersect(set *S, set *T);
