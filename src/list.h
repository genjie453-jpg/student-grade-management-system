#ifndef LIST_H
#define LIST_H

#include "student.h"

/* ===================== 链表节点 ===================== */
/* 采用单向链表：一条记录 = 一个学生的一门课 */
typedef struct Node
{
    Student data;           /* 存储学生信息 */
    struct Node* next;      /* 指向下一个节点的指针 */
} Node;

/* ===================== 插入（返回新的链表头） ===================== */
Node* list_insert_head(Node* head, Student s);
Node* list_insert_tail(Node* head, Student s);

/* ===================== 查找 ===================== */
Node* list_find_by_id(Node* head, const char* id);
/* 按 学号+学期+课程 精确定位一条记录 */
Node* list_find_record(Node* head, const char* id, const char* term, const char* course);
/* 判断 学号+学期+课程 是否已存在（防重复录入），存在返回1 */
int list_exists(Node* head, const char* id, const char* term, const char* course);

/* ===================== 删除 ===================== */
/* 删除该学号的第一条记录，deleted 返回 1/0 表示是否删除成功，返回新链表头 */
Node* list_delete_one(Node* head, const char* id, const char* term, const char* course, int* deleted);

/* ===================== 修改 ===================== */
/* 修改成绩：按 学号+学期+课程 定位，成功返回1 */
int list_update_score(Node* head, const char* id, const char* term, const char* course, float newScore);
/* 统一修改学生基本信息（姓名/班级）：该学号的所有记录一起改，保证一致性 */
int list_update_info(Node* head, const char* id, const char* newName, const char* newClass);

/* ===================== 统计与销毁 ===================== */
int  list_count(Node* head);
void list_print_all(Node* head);
void list_destroy(Node* head);

#endif /* LIST_H */
