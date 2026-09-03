#ifndef LIST_H
#define LIST_H
#include "student.h"
typedef struct Node 
{
    Student data;//这是学生信息
    struct Node* next;
    struct Node* prev;
};

#endif
