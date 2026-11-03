#ifndef INDEX_H
#define INDEX_H

#include "list.h"

/* 哈希索引：学号 -> 链表节点，把按学号查找从 O(n) 降到平均 O(1)。
 * 使用约定：启动时 index_build 一次；链表结构变化（增/删）后必须再次
 * index_build，防止索引里出现悬空指针；index_lookup 未命中时调用方
 * 回退整表遍历兜底。 */

#define INDEX_BUCKETS 256

int   index_build(Node* head);        /* 依据当前链表建索引，返回记录条数 */
Node* index_lookup(const char* id);   /* 查询该学号的第一条记录，找不到返回 NULL */
void  index_destroy(void);            /* 释放索引占用的内存 */

#endif /* INDEX_H */
