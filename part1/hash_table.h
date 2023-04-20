typedef struct list {
    struct list *next;
    void *data;
} list;

typedef struct {
    int bucket;
    list **buckets;
} hashtable;

list *newlist();

hashtable *newtable(int bucket);

void push_back(list *head, void *data);

void add(hashtable *table, void *data, int key_index, int size);

void print_items(list *head);
