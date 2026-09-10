#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "index.h"

/* ---------------- 哈希桶（链地址法解决冲突） ---------------- */

typedef struct IdxEntry {
    char id[MAX_ID_LEN];        /* 学号（关键字） */
    Node* node;                 /* 指向链表中该学号的第一条记录 */
    struct IdxEntry* next;      /* 同一个桶内的冲突链 */
} IdxEntry;

static IdxEntry* s_buckets[INDEX_BUCKETS];

/* FNV-1a 变体哈希：简单、分布均匀、答辩好讲 */
static unsigned int hash_id(const char* id)
{
    unsigned int h = 2166136261u;
    while (*id != '\0') {
        h ^= (unsigned char)(*id++);
        h *= 16777619u;
    }
    return h % INDEX_BUCKETS;
}

int index_build(Node* head)
{
    index_destroy();            /* 重建前先清空旧索引 */

    int count = 0;
    for (Node* p = head; p != NULL; p = p->next) {
        unsigned int b = hash_id(p->data.id);

        /* 同一学号只登记一次（指向第一条记录） */
        int dup = 0;
        for (IdxEntry* e = s_buckets[b]; e != NULL; e = e->next) {
            if (strcmp(e->id, p->data.id) == 0) {
                dup = 1;
                break;
            }
        }
        if (dup)
            continue;

        IdxEntry* e = (IdxEntry*)malloc(sizeof(IdxEntry));
        if (e == NULL)
            break;
        strncpy(e->id, p->data.id, MAX_ID_LEN - 1);
        e->id[MAX_ID_LEN - 1] = '\0';
        e->node = p;
        e->next = s_buckets[b];     /* 头插进桶 */
        s_buckets[b] = e;
        count++;
    }
    return count;
}

Node* index_lookup(const char* id)
{
    if (id == NULL || id[0] == '\0')
        return NULL;
    unsigned int b = hash_id(id);
    for (IdxEntry* e = s_buckets[b]; e != NULL; e = e->next)
        if (strcmp(e->id, id) == 0)
            return e->node;
    return NULL;
}

void index_destroy(void)
{
    for (int i = 0; i < INDEX_BUCKETS; i++) {
        IdxEntry* e = s_buckets[i];
        while (e != NULL) {
            IdxEntry* next = e->next;
            free(e);
            e = next;
        }
        s_buckets[i] = NULL;
    }
}
