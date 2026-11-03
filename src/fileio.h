#ifndef FILEIO_H
#define FILEIO_H

#include "student.h"
#include "list.h"

/* 从文本文件加载记录（一行一条：学号 姓名 班级 学期 课程 分数）
 * 成功返回链表头；文件不存在返回 NULL；返回的条数写入 *outCount（可为 NULL）
 * 说明：按行解析，格式错误的行会被跳过并打印警告，不影响后面的记录。 */
Node* fileio_load(const char* path, int* outCount);

/* 全量保存到文本文件，成功返回保存条数，失败返回 -1
 * 安全保证：先写 <path>.<pid>.tmp 并落盘，再原子替换 <path>；
 * 任何时刻中断都不会破坏原文件，上一版完整数据保留在 <path>.bak。 */
int fileio_save(const char* path, Node* head);

/* 追加单条记录到文件末尾（录入时即时落盘），成功返回 1，失败返回 0
 * 失败时会回滚掉可能写了一半的记录，不会留下半行。 */
int fileio_append(const char* path, const Student* s);

/* 最近一次 fileio_load/save/append 失败的原因（成功后清空），用于提示用户 */
const char* fileio_last_error(void);

#endif /* FILEIO_H */