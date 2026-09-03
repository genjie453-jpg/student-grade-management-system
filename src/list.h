#ifndef LIST_H
#define LIST_H
#include "student.h"
typedef struct Node 
{
    Student data;
    struct Node* next;
    struct Node* prev;
};

#endif
