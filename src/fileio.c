#include <stdio.h>
#include <string.h>
#include "fileio.h"

/* 对应 student.h 的批量导入要求：
 * 文件格式（每行一条，空格分隔）：
 *   学号 姓名 班级 学期 课程 分数
 */
Node* fileio_load(const char* path, int* outCount)
{
    FILE* fp = fopen(path, "r");
    if (fp == NULL)
        return NULL;            /* 首次运行文件不存在，属正常情况 */

    Node* head = NULL;
    Student s;
    int count = 0;

    while (fscanf(fp, "%15s %31s %31s %15s %31s %f",
                  s.id, s.name, s.className, s.term, s.course, &s.score) == 6) {
        head = list_insert_tail(head, s);   /* 尾插保持文件顺序 */
        count++;
    }
    fclose(fp);

    if (outCount != NULL)
        *outCount = count;
    return head;
}

int fileio_save(const char* path, Node* head)
{
    FILE* fp = fopen(path, "w");
    if (fp == NULL)
        return -1;

    int count = 0;
    for (Node* p = head; p != NULL; p = p->next) {
        fprintf(fp, "%s %s %s %s %s %.1f\n",
                p->data.id, p->data.name, p->data.className,
                p->data.term, p->data.course, p->data.score);
        count++;
    }
    fclose(fp);
    return count;
}

int fileio_append(const char* path, const Student* s)
{
    FILE* fp = fopen(path, "a");
    if (fp == NULL)
        return 0;
    fprintf(fp, "%s %s %s %s %s %.1f\n",
            s->id, s->name, s->className, s->term, s->course, s->score);
    fclose(fp);
    return 1;
}
