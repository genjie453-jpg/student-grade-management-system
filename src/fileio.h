#ifndef FILEIO_H
#define FILEIO_H

#include "student.h"
#include "list.h"

/* 从文本文件加载记录（一行一条：学号 姓名 班级 学期 课程 分数）
 * 成功返回链表头；文件不存在返回 NULL；返回的条数写入 *outCount（可为 NULL） */
Node* fileio_load(const char* path, int* outCount);

/* 全量保存到文本文件，成功返回保存条数，失败返回 -1 */
int fileio_save(const char* path, Node* head);

/* 追加单条记录到文件末尾（录入时即时落盘），成功返回 1 */
int fileio_append(const char* path, const Student* s);

#endif /* FILEIO_H */
