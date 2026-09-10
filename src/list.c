#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "list.h"

/* ---------------- 插入 ---------------- */

Node* list_insert_head(Node* head, Student s)
{
    Node* node = (Node*)malloc(sizeof(Node));
    if (node == NULL) {
        printf("[错误] 内存分配失败\n");
        return head;
    }
    node->data = s;
    node->next = head;
    return node;                /* 新节点成为新的头 */
}

Node* list_insert_tail(Node* head, Student s)
{
    Node* node = (Node*)malloc(sizeof(Node));
    if (node == NULL) {
        printf("[错误] 内存分配失败\n");
        return head;
    }
    node->data = s;
    node->next = NULL;

    if (head == NULL)           /* 空表：新节点就是头 */
        return node;

    Node* p = head;
    while (p->next != NULL)     /* 走到最后一个节点 */
        p = p->next;
    p->next = node;
    return head;
}

/* ---------------- 查找 ---------------- */

Node* list_find_by_id(Node* head, const char* id)
{
    for (Node* p = head; p != NULL; p = p->next)
        if (strcmp(p->data.id, id) == 0)
            return p;
    return NULL;
}

Node* list_find_record(Node* head, const char* id, const char* term, const char* course)
{
    for (Node* p = head; p != NULL; p = p->next)
        if (strcmp(p->data.id, id) == 0
            && strcmp(p->data.term, term) == 0
            && strcmp(p->data.course, course) == 0)
            return p;
    return NULL;
}

int list_exists(Node* head, const char* id, const char* term, const char* course)
{
    return list_find_record(head, id, term, course) != NULL;
}

/* ---------------- 删除 ---------------- */

Node* list_delete_one(Node* head, const char* id, const char* term, const char* course, int* deleted)
{
    Node *prev = NULL, *cur = head;
    *deleted = 0;

    while (cur != NULL) {
        if (strcmp(cur->data.id, id) == 0
            && (term == NULL || term[0] == '\0' || strcmp(cur->data.term, term) == 0)
            && (course == NULL || course[0] == '\0' || strcmp(cur->data.course, course) == 0)) {
            if (prev == NULL)       /* 删除的是头节点 */
                head = cur->next;
            else
                prev->next = cur->next;
            free(cur);
            *deleted = 1;
            return head;
        }
        prev = cur;
        cur = cur->next;
    }
    return head;
}

/* ---------------- 修改 ---------------- */

int list_update_score(Node* head, const char* id, const char* term, const char* course, float newScore)
{
    Node* p = list_find_record(head, id, term, course);
    if (p == NULL)
        return 0;
    p->data.score = newScore;
    return 1;
}

int list_update_info(Node* head, const char* id, const char* newName, const char* newClass)
{
    Node* p = list_find_by_id(head, id);
    if (p == NULL)
        return 0;
    for (; p != NULL; p = p->next) {    /* 该学号的所有记录统一改 */
        if (strcmp(p->data.id, id) != 0)
            continue;
        if (newName  && newName[0]  != '\0')
            strncpy(p->data.name, newName, MAX_NAME_LEN - 1);
        if (newClass && newClass[0] != '\0')
            strncpy(p->data.className, newClass, MAX_CLASS_LEN - 1);
        p->data.name[MAX_NAME_LEN - 1] = '\0';
        p->data.className[MAX_CLASS_LEN - 1] = '\0';
    }
    return 1;
}

/* ---------------- 统计与销毁 ---------------- */

int list_count(Node* head)
{
    int n = 0;
    for (Node* p = head; p != NULL; p = p->next)
        n++;
    return n;
}

void list_print_all(Node* head)
{
    printf("%-12s %-10s %-10s %-12s %-16s %s\n",
           "学号", "姓名", "班级", "学期", "课程", "成绩");
    printf("---------------------------------------------------------------\n");
    for (Node* p = head; p != NULL; p = p->next)
        printf("%-12s %-10s %-10s %-12s %-16s %.1f\n",
               p->data.id, p->data.name, p->data.className,
               p->data.term, p->data.course, p->data.score);
    printf("共 %d 条记录\n", list_count(head));
}

void list_destroy(Node* head)
{
    while (head != NULL) {
        Node* next = head->next;
        free(head);
        head = next;
    }
}
