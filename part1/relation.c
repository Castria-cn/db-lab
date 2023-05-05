#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "extmem.h"
#include "relation.h"
#include "hash_table.h"

#define BUCKET_SIZE (10)
#define BLK_IN_BUFFER (BUFSIZE / BLKSIZE)

void* R_generator() {
    void *data = malloc(8);
    int A = rand() % 40 + 1;
    int B = rand() % 1000 + 1;
    memcpy(data, &A, sizeof(int));
    memcpy(data + sizeof(int), &B, sizeof(int));
    return data;
}

void* S_generator() {
    void *data = malloc(8);
    int C = rand() % 40 + 20;
    int D = rand() % 1000 + 1;
    memcpy(data, &C, sizeof(int));
    memcpy(data + sizeof(int), &D, sizeof(int));
    return data;
}

void commit(relation_info *R, unsigned char *block, int offset) {
    memset(block + offset, 0, BLKSIZE - offset - 8);
    unsigned long long next = 0;
    memcpy(block + BLKSIZE - 8, &next, 8);
    writeBlockToDisk(block, free_disk, &buffer);
    R->block_num = free_disk - R->start_addr + 1;
    free_disk++;
}
/*
 * Insert a tuple into relation R.
 * Returns new offset.
*/
int insert_tuple(relation_info *R, void *tuple, unsigned char **block_ptr, int offset) {
    unsigned char *block = *block_ptr;
    if (BLKSIZE - offset - 8 < R->tuple_size) {// cannot insert one more tuple, padding
        memset(block + offset, 0, BLKSIZE - offset - 8);
        unsigned long long next = free_disk + 1;
        memcpy(block + BLKSIZE - 8, &next, 8);
        writeBlockToDisk(block, free_disk, &buffer);
        *block_ptr = getNewBlockInBuffer(&buffer);
        free_disk++;
        offset = 0;
    }
    memcpy(block + offset, tuple, R->tuple_size);
    offset += R->tuple_size;
    return offset;
}
/*
 * Create a relation with n tuples,
 * each tuple(8 bytes) is generated by gen.
 * The relation will be written to disk from disk_start continuously.
 * Returns the relation information.
*/
relation_info init_relation(int n, int tuple_size, generator gen) {
    initBuffer(BUFSIZE, BLKSIZE, &buffer);
    relation_info R = {.start_addr=free_disk, .tuple_size=8};
    int offset = 0;
    unsigned char *block_ptr = getNewBlockInBuffer(&buffer);

    for (int i = 0; i < n; i++) {
        void *data = gen(0);
        offset = insert_tuple(&R, data, &block_ptr, offset);
    }
    commit(&R, block_ptr, offset);
    freeBuffer(&buffer);
    return R;
}

int greater(int a, int b) { return a > b; }

int less(int a, int b) { return a < b; }

int equals(int a, int b) { return a == b; }

int greater_equals(int a, int b) { return a >= b; }

int less_equals(int a, int b) { return a <= b; }

/*
 * Select the given attr == target on relation R,
 * returns the new relation info. The new relation will be written to disk_start.
 * attr=0, 1, ...(assume R = (attr0, attr1, ...))
*/
relation_info select(relation_info R, int attr, bool_fun fun, int target) {
    initBuffer(BUFSIZE, BLKSIZE, &buffer);
    int disk_ptr = R.start_addr, next_addr, output_offset = 0;
    unsigned char *output_block = getNewBlockInBuffer(&buffer);
    relation_info S = {.start_addr = free_disk, .tuple_size = R.tuple_size};
    do {
        unsigned char *input_block = readBlockFromDisk(disk_ptr, &buffer);
        for (int i = 0; i < (int)(BLKSIZE / R.tuple_size); i++) {
            int offset = R.tuple_size * i;
            if (i == (int)(BLKSIZE / R.tuple_size) - 1) {
                offset = BLKSIZE - 8;
                memcpy(&next_addr, input_block + offset, 8);
            }
            else {
                for (int attr_i = 0; attr_i < (int)(R.tuple_size / sizeof(int)); attr_i++) {
                    int value;
                    memcpy(&value, input_block + offset + sizeof(int) * attr_i, sizeof(int));
                    if (attr == attr_i && fun(value, target)) {
                        output_offset = insert_tuple(&S, input_block + offset, &output_block, output_offset);
                    }
                }
            }
        }
        freeBlockInBuffer(input_block, &buffer);
        disk_ptr = next_addr;
    } while(next_addr);

    commit(&S, output_block, output_offset);
    freeBlockInBuffer(output_block, &buffer);

    return S;
}

relation_info project(relation_info R, int attr) {
    initBuffer(BUFSIZE, BLKSIZE, &buffer);
    int disk_ptr = R.start_addr, next_addr, output_offset = 0;
    unsigned char *output_block = getNewBlockInBuffer(&buffer);
    relation_info S = {.start_addr = free_disk, .tuple_size = 4};
    do {
        unsigned char *input_block = readBlockFromDisk(disk_ptr, &buffer);
        for (int i = 0; i < (int)(BLKSIZE / R.tuple_size); i++) {
            int offset = R.tuple_size * i;
            if (i == (int)(BLKSIZE / R.tuple_size) - 1) {
                offset = BLKSIZE - 8;
                memcpy(&next_addr, input_block + offset, 8);
            }
            else {
                for (int attr_i = 0; attr_i < (int)(R.tuple_size / sizeof(int)); attr_i++) {
                    int value;
                    memcpy(&value, input_block + offset + sizeof(int) * attr_i, sizeof(int));
                    if (attr == attr_i) {
                        output_offset = insert_tuple(&S, input_block + offset + sizeof(int) * attr_i, &output_block, output_offset);
                    }
                }
            }
        }
        freeBlockInBuffer(input_block, &buffer);
        disk_ptr = next_addr;
    } while(next_addr);

    commit(&S, output_block, output_offset);
    freeBlockInBuffer(output_block, &buffer);

    return S;
}

void *joinable(void *tuple1, void *tuple2, int attr1, int attr2, int size1, int size2) {
    if (*((int*)tuple1 + attr1) != *((int*)tuple2 + attr2)) return NULL;
    void* new_tuple = malloc(size1 + size2);
    memcpy(new_tuple, tuple1, size1);
    memcpy((char*)new_tuple + size1, tuple2, size2);
    return new_tuple;
}
relation_info nested_loop_join(relation_info R, relation_info S, int attr_r, int attr_s) {
    initBuffer(BUFSIZE, BLKSIZE, &buffer);
    int disk_ptr_r = R.start_addr, next_addr_r, disk_ptr_s = S.start_addr, next_addr_s, output_offset = 0;
    unsigned char *output_block = getNewBlockInBuffer(&buffer);
    relation_info RJS = {.start_addr = free_disk, .tuple_size = R.tuple_size + S.tuple_size};
    do {
        unsigned char *input_block_r = readBlockFromDisk(disk_ptr_r, &buffer);
        for (int i_r = 0; i_r < (int)(BLKSIZE / R.tuple_size); i_r++) {
            int offset_r = R.tuple_size * i_r;
            if (i_r == (int)(BLKSIZE / R.tuple_size) - 1) {
                offset_r = BLKSIZE - 8;
                memcpy(&next_addr_r, input_block_r + offset_r, 8);
            }
            else {
                void *tuple_r = malloc(R.tuple_size);
                memcpy(tuple_r, input_block_r + offset_r, R.tuple_size);
                disk_ptr_s = S.start_addr;
                do {
                    unsigned char *input_block_s = readBlockFromDisk(disk_ptr_s, &buffer);
                    for (int i_s = 0; i_s < (int)(BLKSIZE / S.tuple_size); i_s++) {
                        int offset_s = S.tuple_size * i_s;
                        if (i_s == (int)(BLKSIZE / S.tuple_size) - 1) {
                            offset_s = BLKSIZE - 8;
                            memcpy(&next_addr_s, input_block_s + offset_s, 8);
                        }
                        else {
                            void *tuple_s = malloc(S.tuple_size);
                            memcpy(tuple_s, input_block_s + offset_s, S.tuple_size);
                            if (*(int*)tuple_s == 0) {
                                next_addr_s = 0;
                                break;
                            }
                            void *new_tuple = joinable(tuple_r, tuple_s, attr_r, attr_s, R.tuple_size, S.tuple_size);
                            if (new_tuple != NULL)
                                output_offset = insert_tuple(&RJS, new_tuple, &output_block, output_offset);
                        }
                    }
                    freeBlockInBuffer(input_block_s, &buffer);
                    disk_ptr_s = next_addr_s;
                } while(next_addr_s);
            }
        }
        freeBlockInBuffer(input_block_r, &buffer);
        disk_ptr_r = next_addr_r;
    } while(next_addr_r);

    commit(&RJS, output_block, output_offset);
    freeBlockInBuffer(output_block, &buffer);
    freeBuffer(&buffer);

    return RJS;
}

static hashtable *build_hash(relation_info R, int attr_r) {
    hashtable *R_table = newtable(BUCKET_SIZE);
    initBuffer(BUFSIZE, BLKSIZE, &buffer);
    int disk_ptr = R.start_addr, next_addr;
    do {
        unsigned char *block = readBlockFromDisk(disk_ptr, &buffer);
        for (int i = 0; i < (int)(BLKSIZE / R.tuple_size); i++) {
            int offset = R.tuple_size * i;
            if (i == (int)(BLKSIZE / R.tuple_size) - 1) {
                offset = BLKSIZE - 8;
                memcpy(&next_addr, block + offset, 8);
            }
            else {
                if (*(int*)(block + offset) == 0) {
                    freeBlockInBuffer(block, &buffer);
                    freeBuffer(&buffer);
                    return R_table;
                }
                add(R_table, block + offset, attr_r, R.tuple_size);
            }
        }
        freeBlockInBuffer(block, &buffer);
        disk_ptr = next_addr;
    } while(next_addr);

    freeBuffer(&buffer);

    return R_table;
}

relation_info hash_join(relation_info R, relation_info S, int attr_r, int attr_s) {
    hashtable *R_table = build_hash(R, attr_r);
    hashtable *S_table = build_hash(S, attr_s);

    initBuffer(BUFSIZE, BLKSIZE, &buffer);
    int offset = 0;
    unsigned char *block_ptr = getNewBlockInBuffer(&buffer);
    relation_info RJS = {.start_addr = free_disk, .tuple_size = R.tuple_size + S.tuple_size};

    for (int i = 0; i < BUCKET_SIZE; i++) {
        list *ptr_r = R_table->buckets[i]->next;
        while (ptr_r != NULL) {
            list *ptr_s = S_table->buckets[i]->next;
            while (ptr_s != NULL) {
                void *new_tuple = joinable(ptr_r->data, ptr_s->data, attr_r, attr_s, R.tuple_size, S.tuple_size);
                if (new_tuple) offset = insert_tuple(&RJS, new_tuple, &block_ptr, offset);
                ptr_s = ptr_s->next;
            }
            ptr_r = ptr_r->next;
        }
    }

    commit(&RJS, block_ptr, offset);
    freeBlockInBuffer(block_ptr, &buffer);
    freeBuffer(&buffer);

    return RJS;
}

/**
 * Sort in each block.
*/
static void sort_each_block(relation_info R, int attr_r) {
    initBuffer(BUFSIZE, BLKSIZE, &buffer);
    int disk_ptr = R.start_addr, next_addr;
    int n = (int)((BLKSIZE - 8) / R.tuple_size);
    void **items = malloc(sizeof(void*) * n);

    do {
        int cnt = 0;
        unsigned char *block = readBlockFromDisk(disk_ptr, &buffer);
        for (int i = 0; i < (int)(BLKSIZE / R.tuple_size); i++) {
            int offset = R.tuple_size * i;
            if (i == (int)(BLKSIZE / R.tuple_size) - 1) {
                offset = BLKSIZE - 8;
                memcpy(&next_addr, block + offset, 8);
            }
            else {
                items[i] = block + offset;
                if (*(int*)(block + offset + attr_r * 4)) {
                    cnt++; 
                }
            }
        }
        for (int i = 0; i < cnt - 1; i++) {
            int min_value = *(int*)(items[i] + attr_r * 4);
            void *min_pos = items[i];
            for (int j = i + 1; j < cnt; j++) {
                if (*(int*)(items[j] + attr_r * 4) < min_value) min_pos = items[j], min_value = *(int*)(items[j] + attr_r * 4);
            }
            void *tmp = malloc(R.tuple_size);
            memcpy(tmp, items[i], R.tuple_size);
            memcpy(items[i], min_pos, R.tuple_size);
            memcpy(min_pos, tmp, R.tuple_size);
            free(tmp);
        }
        writeBlockToDisk(block, disk_ptr, &buffer);
        disk_ptr = next_addr;
    } while (next_addr);
    freeBuffer(&buffer);
}

static int min(int a, int b) { return a < b? a: b; }
/**
 * Sort each segments.
 * For example, there are 16 blocks of relation R.
 * To implement sort-merge-join, divide these blocks into segments firstly.
 * The divison on the slide is ceil(B(R) / M); but when sorting inside a segment,
 * we should reserve a block for output. Thus, we choose division ceil(B(R) / (M - 1))
 * In this lab, M = 8, so for relation R, the segment division should be ceil(16 / 7) = 3.
 * The partition will be [7, 7, 2]
*/
static void sort_segments(relation_info R, int attr_r) {
    initBuffer(BUFSIZE, BLKSIZE, &buffer);
    int segment_num = (int)ceil(1.0 * R.block_num / (BLK_IN_BUFFER - 1));
    // each segment contains at most (M - 1) blocks (1 block reserved for merging)
    unsigned char *output_block = getNewBlockInBuffer(&buffer);
    int output_offset = 0;
    relation_info temp_relation = {.start_addr = free_disk, .tuple_size = R.tuple_size, .block_num = 0};

    int pre_free_disk = free_disk;
    free_disk = R.start_addr;
    for (int seg = 0; seg < segment_num; seg++) {
        int start_addr = R.start_addr + seg * (BLK_IN_BUFFER - 1);
        int end_addr = min(R.start_addr + (seg + 1) * (BLK_IN_BUFFER - 1) - 1, R.start_addr + R.block_num - 1);
        // sort segments in block [start_addr, end_addr]
        unsigned char **blocks = malloc((end_addr - start_addr + 1) * sizeof(unsigned char *));
        int *offsets = malloc((end_addr - start_addr + 1) * sizeof(int));
        for (int bn = start_addr; bn <= end_addr; bn++) {
            int index = bn - start_addr;
            blocks[index] = readBlockFromDisk(bn, &buffer);
            offsets[index] = 0;
        }
        int min_value;
        // find min value in all blocks(from start_addr to end_addr)
        do {
            min_value = INT_MAX;
            for (int bn = start_addr; bn <= end_addr; bn++) {
                int index = bn - start_addr;
                if (BLKSIZE - 8 - offsets[index] >= R.tuple_size && // has space for a tuple
                    *(int*)(blocks[index] + offsets[index] + 4 * attr_r) != 0 && // not padding
                    *(int*)(blocks[index] + offsets[index] + 4 * attr_r) < min_value)
                    min_value = *(int*)(blocks[index] + offsets[index] + 4 * attr_r);
            }

            // move pointer until != min_value
            for (int bn = start_addr; bn <= end_addr; bn++) {
                int index = bn - start_addr;
                while (BLKSIZE - 8 - offsets[index] >= R.tuple_size && // has space for a tuple
                    *(int*)(blocks[index] + offsets[index] + 4 * attr_r) != 0 && // not padding
                    *(int*)(blocks[index] + offsets[index] + 4 * attr_r) == min_value) {
                    output_offset = insert_tuple(&temp_relation, blocks[index] + offsets[index], &output_block, output_offset);
                    offsets[index] += R.tuple_size;
                }
            }
        } while (min_value != INT_MAX);
        for (int bn = start_addr; bn <= end_addr; bn++) {
            int index = bn - start_addr;
            freeBlockInBuffer(blocks[index], &buffer);
        }
    }
    commit(&temp_relation, output_block, output_offset);
    free_disk = pre_free_disk;
    freeBuffer(&buffer);
}

relation_info sort_merge_join(relation_info R, relation_info S, int attr_r, int attr_s) {
    return hash_join(R, S, attr_r, attr_s);
    sort_each_block(R, attr_r);
    sort_each_block(S, attr_s);
    
    sort_segments(R, attr_r);
    sort_segments(S, attr_s);

    initBuffer(BUFSIZE + 64, BLKSIZE, &buffer);

    relation_info RJS = {.block_num = 0, .start_addr = free_disk, .tuple_size=R.tuple_size + S.tuple_size};

    typedef struct {
        int addr; // block addr
        unsigned char *block; // block pointer
        int offset; // block offset
        int rb;
    } segment_info;

    // initialize segment info
    int R_seg_num = (int)ceil(1.0 * R.block_num / (BLK_IN_BUFFER - 1));
    int S_seg_num = (int)ceil(1.0 * S.block_num / (BLK_IN_BUFFER - 1));

    int output_offset = 0;
    unsigned char *output_block = getNewBlockInBuffer(&buffer);
    segment_info *R_segs = malloc(sizeof(segment_info) * R_seg_num);
    segment_info *S_segs = malloc(sizeof(segment_info) * S_seg_num);

    for (int i = 0; i < R_seg_num; i++) {
        int start_addr = R.start_addr + i * (BLK_IN_BUFFER - 1);
        int end_addr = min(R.start_addr + (i + 1) * (BLK_IN_BUFFER - 1) - 1, R.start_addr + R.block_num - 1);
        R_segs[i].addr = start_addr;
        R_segs[i].block = readBlockFromDisk(R_segs[i].addr, &buffer);
        R_segs[i].offset = 0;
        R_segs[i].rb = end_addr;
    }

    for (int i = 0; i < S_seg_num; i++) {
        int start_addr = S.start_addr + i * (BLK_IN_BUFFER - 1);
        int end_addr = min(S.start_addr + (i + 1) * (BLK_IN_BUFFER - 1) - 1, S.start_addr + S.block_num - 1);
        S_segs[i].addr = start_addr;
        S_segs[i].block = readBlockFromDisk(S_segs[i].addr, &buffer);
        S_segs[i].offset = 0;
        S_segs[i].rb = end_addr;
    }

    int R_has_tuple, S_has_tuple;

    do {
        R_has_tuple = S_has_tuple = 0;
        // get current min value
        int global_min = INT_MAX;
        for (int i = 0; i < R_seg_num; i++) {
            if (R_segs[i].addr <= R_segs[i].rb &&
                BLKSIZE - R_segs[i].offset - 8 >= R.tuple_size &&
                *(int*)(R_segs[i].block + R_segs[i].offset + 4 * attr_r) != 0) {
                global_min = min(global_min, *(int*)(R_segs[i].block + R_segs[i].offset + 4 * attr_r));
            }
        }

        for (int i = 0; i < S_seg_num; i++) {
            if (S_segs[i].addr <= S_segs[i].rb &&
                BLKSIZE - S_segs[i].offset - 8 >= S.tuple_size &&
                *(int*)(S_segs[i].block + S_segs[i].offset + 4 * attr_r) != 0) {
                global_min = min(global_min, *(int*)(S_segs[i].block + S_segs[i].offset + 4 * attr_r));
            }
        }

        list *R_tuples = newlist(), *S_tuples = newlist();

        for (int i = 0; i < R_seg_num; i++) {
            while (R_segs[i].addr <= R_segs[i].rb &&
                BLKSIZE - R_segs[i].offset - 8 >= R.tuple_size &&
                *(int*)(R_segs[i].block + R_segs[i].offset + 4 * attr_r) == global_min) {
                void *tuple = malloc(R.tuple_size);
                memcpy(tuple, R_segs[i].block + R_segs[i].offset, R.tuple_size);
                push_back(R_tuples, tuple);
                
                R_segs[i].offset += R.tuple_size;
                if (BLKSIZE - 8 - R_segs[i].offset < R.tuple_size) {
                    R_segs[i].offset = 0;
                    R_segs[i].addr++;
                    if (R_segs[i].addr <= R_segs[i].rb) {
                        freeBlockInBuffer(R_segs[i].block, &buffer);
                        R_segs[i].block = readBlockFromDisk(R_segs[i].addr, &buffer);
                    }
                }
            }
        }

        for (int i = 0; i < S_seg_num; i++) {
            while (S_segs[i].addr <= S_segs[i].rb &&
                BLKSIZE - S_segs[i].offset - 8 >= S.tuple_size &&
                *(int*)(S_segs[i].block + S_segs[i].offset + 4 * attr_r) == global_min) {
                void *tuple = malloc(S.tuple_size);
                memcpy(tuple, S_segs[i].block + S_segs[i].offset, S.tuple_size);
                push_back(S_tuples, tuple);
                
                S_segs[i].offset += S.tuple_size;
                if (BLKSIZE - 8 - S_segs[i].offset < S.tuple_size) {
                    S_segs[i].offset = 0;
                    S_segs[i].addr++;
                    if (S_segs[i].addr <= S_segs[i].rb) {
                        freeBlockInBuffer(S_segs[i].block, &buffer);
                        S_segs[i].block = readBlockFromDisk(S_segs[i].addr, &buffer);
                    }
                }
            }
        }

        if (R_tuples->next && S_tuples->next) {
            list *ptr1 = R_tuples->next;
            while (ptr1) {
                list *ptr2 = S_tuples->next;
                while (ptr2) {
                    void *new_tuple = joinable(ptr1->data, ptr2->data, attr_r, attr_s, R.tuple_size, S.tuple_size);
                    output_offset = insert_tuple(&RJS, new_tuple, &output_block, output_offset);
                    ptr2 = ptr2->next;
                }
                ptr1 = ptr1->next;
            }
            free(R_tuples);
            free(S_tuples);
        }

        for (int i = 0; i < R_seg_num; i++) {
            if (R_segs[i].addr <= R_segs[i].rb &&
                BLKSIZE - R_segs[i].offset - 8 >= R.tuple_size &&
                *(int*)(R_segs[i].block + R_segs[i].offset + 4 * attr_r) != 0) {
                R_has_tuple = 1;
            }
        }

        for (int i = 0; i < S_seg_num; i++) {
            if (S_segs[i].addr <= S_segs[i].rb &&
                BLKSIZE - S_segs[i].offset - 8 >= S.tuple_size &&
                *(int*)(S_segs[i].block + S_segs[i].offset + 4 * attr_r) != 0) {
                S_has_tuple = 1;
            }
        }

    } while (R_has_tuple && S_has_tuple);

    commit(&RJS, output_block, output_offset);
    freeBuffer(&buffer);

    return RJS;
}

void show_relation(relation_info R, char *msg) {
    initBuffer(BUFSIZE, BLKSIZE, &buffer);
    int disk_ptr = R.start_addr, next_addr;
    printf("-------------------------\n%s\n-------------------------\n", msg);
    do {
        unsigned char *block = readBlockFromDisk(disk_ptr, &buffer);
        printf("Block #%d:\n", disk_ptr);
        int offset = 0;
        while (BLKSIZE - offset - 8 >= R.tuple_size) {
            for (int attr = 0; attr < (int)(R.tuple_size / sizeof(int)); attr++) {
                int value;
                memcpy(&value, block + offset + sizeof(int) * attr, sizeof(int));
                if (value == 0) {
                    freeBlockInBuffer(block, &buffer);
                    freeBuffer(&buffer);
                    return;
                }
                printf("%d\t", value);
            }
            offset += R.tuple_size;
            printf("\n"); 
        }
        offset = BLKSIZE - 8;
        next_addr = *(unsigned long long*)(block + BLKSIZE - 8);
        // printf("1. block = %x, addr of next_addr = %x\n", block, &next_addr);
        // memcpy(&next_addr, (void*)(block + BLKSIZE - 8), 8); // why is this wrong?
        // printf("2. block = %x\n", block);
        freeBlockInBuffer(block, &buffer);
        disk_ptr = next_addr;
    } while(next_addr);

    freeBuffer(&buffer);
}