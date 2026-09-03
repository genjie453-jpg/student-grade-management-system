#ifndef STUDENT_H
#define STUDENT_H

/* ===================== 常量定义 ===================== */
#define MAX_ID_LEN      16      /* 学号最大长度 */
#define MAX_NAME_LEN    32      /* 姓名最大长度 */
#define MAX_CLASS_LEN   32      /* 班级最大长度，如 "计科2301" */
#define MAX_TERM_LEN    16      /* 学期最大长度，如 "2025-2026-1" */
#define MAX_COURSE_LEN  32      /* 课程名最大长度 */
#define MAX_LINE_LEN    256     /* 文件读取单行缓冲区大小 */

#define PASS_SCORE      60.0f   /* 及格分数线 */

/* 数据文件路径（相对程序运行目录） */
#define DATA_FILE       "data/students.txt"

/* ===================== 用户角色 ===================== */
/* 对应指导书要求：管理员、教师、学生三种角色权限控制 */
typedef enum {
    ROLE_ADMIN = 0,     /* 管理员：增删改查全部功能 */
    ROLE_TEACHER = 1,   /* 教师：录入、修改成绩，查询统计 */
    ROLE_STUDENT = 2    /* 学生：仅查询本人成绩 */
} Role;

/* ===================== 学生成绩记录 ===================== */
/*
 * 设计说明：一条记录 = 一个学生的一门课在一个学期的成绩。
 * 这样按课程/学期/班级多条件查询时直接遍历匹配即可，
 * 排名统计也方便（按课程+学期筛选后排序）。
 */
typedef struct Student {
    char    id[MAX_ID_LEN];         /* 学号（关键字，不可重复） */
    char    name[MAX_NAME_LEN];     /* 姓名 */
    char    className[MAX_CLASS_LEN];   /* 班级 */
    char    term[MAX_TERM_LEN];     /* 学期，如 "2025-2026-1" */
    char    course[MAX_COURSE_LEN]; /* 课程名 */
    float   score;                  /* 成绩 0~100 */
} Student;

#endif /* STUDENT_H */
