#ifndef LIST_H
#define LIST_H
#include "student.h"
typedef struct Node 
{
    Student data;// 存储学生信息
    struct Node* next;// 指向下一个节点的指针
    struct Node* prev;// 指向上一个节点的指针
} Node;

#endif
