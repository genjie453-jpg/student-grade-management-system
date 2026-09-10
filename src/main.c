/* 学生成绩管理系统 - 主程序
 * 启动方式：双击运行后选择模式；
 * 命令行参数 "web" 可直接进入页面模式（供快捷方式使用） */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "student.h"
#include "list.h"
#include "fileio.h"
#include "index.h"
#include "auth.h"
#include "webui.h"

static Node* g_head = NULL;     /* 全部成绩记录（链表） */
static int   g_role = ROLE_ADMIN;

/* ---------------- 控制台输入辅助 ---------------- */

/* 读取一行输入（含空格），去掉行尾换行 */
static void read_line(char* buf, int size)
{
    if (fgets(buf, size, stdin) == NULL) {
        buf[0] = '\0';
        return;
    }
    buf[strcspn(buf, "\r\n")] = '\0';
}

/* 录入一条记录（教师/管理员） */
static void flow_add(void)
{
    Student s = {0};
    char buf[64];

    printf("学号: ");    read_line(s.id, MAX_ID_LEN);
    printf("姓名: ");    read_line(s.name, MAX_NAME_LEN);
    printf("班级: ");    read_line(s.className, MAX_CLASS_LEN);
    printf("学期(如2025-2026-1): "); read_line(s.term, MAX_TERM_LEN);
    printf("课程: ");    read_line(s.course, MAX_COURSE_LEN);
    printf("分数(0~100): ");
    read_line(buf, sizeof(buf));
    s.score = (float)atof(buf);

    if (s.id[0] == '\0' || s.term[0] == '\0' || s.course[0] == '\0') {
        printf("[失败] 学号/学期/课程不能为空\n");
        return;
    }
    if (s.score < 0 || s.score > 100) {
        printf("[失败] 成绩必须在 0~100 之间\n");
        return;
    }
    if (list_exists(g_head, s.id, s.term, s.course)) {
        printf("[失败] 该学号在本学期的这门课已存在\n");
        return;
    }
    g_head = list_insert_tail(g_head, s);
    fileio_append(DATA_FILE, &s);
    printf("[成功] 已录入并写入文件\n");
}

/* 统计分析：按 课程(+学期) 输出平均分/及格率/排名表 */
static void flow_stats(void)
{
    char course[MAX_COURSE_LEN], term[MAX_TERM_LEN];
    printf("课程(留空=全部): ");  read_line(course, sizeof(course));
    printf("学期(留空=全部): ");  read_line(term, sizeof(term));

    static Student arr[2048];
    int n = 0;
    for (Node* p = g_head; p != NULL && n < 2048; p = p->next) {
        if (course[0] != '\0' && strcmp(p->data.course, course) != 0) continue;
        if (term[0]   != '\0' && strcmp(p->data.term, term)   != 0) continue;
        arr[n++] = p->data;
    }
    if (n == 0) {
        printf("无匹配记录\n");
        return;
    }

    /* 简单选择排序：分数降序（体现"排序"知识点） */
    for (int i = 0; i < n - 1; i++) {
        int max = i;
        for (int j = i + 1; j < n; j++)
            if (arr[j].score > arr[max].score)
                max = j;
        if (max != i) {
            Student t = arr[i]; arr[i] = arr[max]; arr[max] = t;
        }
    }

    double sum = 0.0;
    int pass = 0;
    for (int i = 0; i < n; i++) {
        sum += arr[i].score;
        if (arr[i].score >= PASS_SCORE)
            pass++;
    }

    printf("\n===== 统计结果 =====\n");
    printf("人数: %d   平均分: %.2f   及格: %d 人   及格率: %.1f%%\n\n",
           n, sum / n, pass, pass * 100.0 / n);
    printf("%-6s %-12s %-10s %-16s %s\n", "名次", "学号", "姓名", "课程", "成绩");
    for (int i = 0; i < n; i++) {
        int rank = 1;
        for (int j = 0; j < i; j++)
            if (arr[j].score > arr[i].score)
                rank++;
        printf("%-6d %-12s %-10s %-16s %.1f\n",
               rank, arr[i].id, arr[i].name, arr[i].course, arr[i].score);
    }
}

/* 查询：按学号/姓名/班级/学期/课程多条件 */
static void flow_query(void)
{
    char id[MAX_ID_LEN], name[MAX_NAME_LEN], cls[MAX_CLASS_LEN];
    char term[MAX_TERM_LEN], course[MAX_COURSE_LEN];
    char buf[64];

    printf("学号(留空跳过): ");   read_line(id, sizeof(id));
    printf("姓名(留空跳过): ");   read_line(name, sizeof(name));
    printf("班级(留空跳过): ");   read_line(cls, sizeof(cls));
    printf("学期(留空跳过): ");   read_line(term, sizeof(term));
    printf("课程(留空跳过): ");   read_line(course, sizeof(course));
    (void)buf;

    printf("\n");
    printf("%-12s %-10s %-10s %-12s %-16s %s\n",
           "学号", "姓名", "班级", "学期", "课程", "成绩");
    printf("---------------------------------------------------------------\n");
    int n = 0;
    for (Node* p = g_head; p != NULL; p = p->next) {
        if (id[0]     != '\0' && strcmp(p->data.id, id) != 0) continue;
        if (name[0]   != '\0' && strcmp(p->data.name, name) != 0) continue;
        if (cls[0]    != '\0' && strcmp(p->data.className, cls) != 0) continue;
        if (term[0]   != '\0' && strcmp(p->data.term, term) != 0) continue;
        if (course[0] != '\0' && strcmp(p->data.course, course) != 0) continue;
        printf("%-12s %-10s %-10s %-12s %-16s %.1f\n",
               p->data.id, p->data.name, p->data.className,
               p->data.term, p->data.course, p->data.score);
        n++;
    }
    printf("匹配 %d 条\n", n);
}

static void flow_update_score(void)
{
    char id[MAX_ID_LEN], term[MAX_TERM_LEN], course[MAX_COURSE_LEN], buf[64];
    printf("学号: ");    read_line(id, sizeof(id));
    printf("学期: ");    read_line(term, sizeof(term));
    printf("课程: ");    read_line(course, sizeof(course));
    printf("新分数(0~100): "); read_line(buf, sizeof(buf));
    float score = (float)atof(buf);

    if (score < 0 || score > 100) {
        printf("[失败] 成绩必须在 0~100 之间\n");
        return;
    }
    if (list_update_score(g_head, id, term, course, score)) {
        fileio_save(DATA_FILE, g_head);
        printf("[成功] 已修改并保存\n");
    } else {
        printf("[失败] 未找到匹配记录\n");
    }
}

static void flow_update_info(void)
{
    char id[MAX_ID_LEN], name[MAX_NAME_LEN], cls[MAX_CLASS_LEN];
    printf("学号: ");            read_line(id, sizeof(id));
    printf("新姓名(留空不变): "); read_line(name, sizeof(name));
    printf("新班级(留空不变): "); read_line(cls, sizeof(cls));

    if (list_update_info(g_head, id, name, cls)) {
        fileio_save(DATA_FILE, g_head);
        printf("[成功] 该学号的所有记录已统一更新并保存\n");
    } else {
        printf("[失败] 学号不存在\n");
    }
}

static void flow_delete(void)
{
    char id[MAX_ID_LEN], term[MAX_TERM_LEN], course[MAX_COURSE_LEN];
    printf("学号: ");   read_line(id, sizeof(id));
    printf("学期(留空=该生全部): "); read_line(term, sizeof(term));
    printf("课程(留空=全部): ");     read_line(course, sizeof(course));

    int deleted = 0;
    int before = list_count(g_head);
    /* 逐条删除：term/course 为空时删除该生全部匹配记录 */
    for (;;) {
        g_head = list_delete_one(g_head, id, term, course, &deleted);
        if (!deleted) break;
    }
    int removed = before - list_count(g_head);
    if (removed > 0) {
        fileio_save(DATA_FILE, g_head);
        printf("[成功] 删除 %d 条并保存\n", removed);
    } else {
        printf("[失败] 未找到匹配记录\n");
    }
}

static void flow_save(void)
{
    int n = fileio_save(DATA_FILE, g_head);
    printf(n >= 0 ? "[成功] 已保存 %d 条到 %s\n" : "[失败] 无法写入文件\n",
           n, DATA_FILE);
}

/* ---------------- 角色菜单 ---------------- */

static void admin_menu(void)
{
    for (;;) {
        printf("\n===== 管理员菜单 =====\n"
               "1. 录入成绩\n2. 修改成绩\n3. 修改学生信息\n4. 删除记录\n"
               "5. 多条件查询\n6. 统计分析(排名)\n7. 显示全部\n"
               "8. 保存文件\n0. 退出\n请选择: ");
        char buf[8];
        read_line(buf, sizeof(buf));
        switch (buf[0]) {
        case '1': flow_add(); break;
        case '2': flow_update_score(); break;
        case '3': flow_update_info(); break;
        case '4': flow_delete(); break;
        case '5': flow_query(); break;
        case '6': flow_stats(); break;
        case '7': list_print_all(g_head); break;
        case '8': flow_save(); break;
        case '0': return;
        default:  printf("无效选择\n");
        }
    }
}

static void teacher_menu(void)
{
    for (;;) {
        printf("\n===== 教师菜单 =====\n"
               "1. 录入成绩\n2. 修改成绩\n3. 多条件查询\n4. 统计分析(排名)\n"
               "5. 显示全部\n6. 保存文件\n0. 退出\n请选择: ");
        char buf[8];
        read_line(buf, sizeof(buf));
        switch (buf[0]) {
        case '1': flow_add(); break;
        case '2': flow_update_score(); break;
        case '3': flow_query(); break;
        case '4': flow_stats(); break;
        case '5': list_print_all(g_head); break;
        case '6': flow_save(); break;
        case '0': return;
        default:  printf("无效选择\n");
        }
    }
}

static void student_menu(const char* id)
{
    for (;;) {
        printf("\n===== 学生菜单 (本人: %s) =====\n"
               "1. 查询我的成绩\n2. 我的单科统计\n0. 退出\n请选择: ", id);
        char buf[8];
        read_line(buf, sizeof(buf));
        if (buf[0] == '1') {
            printf("\n");
            printf("%-12s %-10s %-12s %-16s %s\n", "学期", "姓名", "班级", "课程", "成绩");
            printf("-----------------------------------------------\n");
            for (Node* p = g_head; p != NULL; p = p->next)
                if (strcmp(p->data.id, id) == 0)
                    printf("%-12s %-10s %-12s %-16s %.1f\n",
                           p->data.term, p->data.name, p->data.className,
                           p->data.course, p->data.score);
        } else if (buf[0] == '2') {
            /* 该生成绩单统计：总分/平均分/挂科数 */
            double sum = 0.0;
            int n = 0, failed = 0;
            printf("学期(留空=全部): ");
            char term[MAX_TERM_LEN];
            read_line(term, sizeof(term));
            for (Node* p = g_head; p != NULL; p = p->next) {
                if (strcmp(p->data.id, id) != 0) continue;
                if (term[0] != '\0' && strcmp(p->data.term, term) != 0) continue;
                sum += p->data.score;
                n++;
                if (p->data.score < PASS_SCORE)
                    failed++;
            }
            if (n == 0)
                printf("无记录\n");
            else
                printf("共 %d 门  总分 %.1f  平均分 %.2f  挂科 %d 门\n",
                       n, sum, sum / n, failed);
        } else if (buf[0] == '0') {
            return;
        } else {
            printf("无效选择\n");
        }
    }
}

static void console_mode(void)
{
    /* 登录：统一使用账号文件 data/users.txt 验证 */
    char account[MAX_ACCOUNT], pwd[MAX_PWD];
    Session sess;

    printf("登录（学生账号=学号，默认密码见 README）\n");
    printf("账号: "); read_line(account, sizeof(account));
    printf("密码: "); read_line(pwd, sizeof(pwd));

    if (!auth_login(account, pwd, &sess)) {
        printf("账号或密码错误\n");
        return;
    }
    g_role = sess.role;
    printf("欢迎，%s（%s）\n", sess.name,
           sess.role == ROLE_ADMIN ? "管理员" : sess.role == ROLE_TEACHER ? "教师" : "学生");

    if (sess.role == ROLE_ADMIN)
        admin_menu();
    else if (sess.role == ROLE_TEACHER)
        teacher_menu();
    else
        student_menu(sess.id);
}

/* ---------------- 入口 ---------------- */

int main(int argc, char* argv[])
{
    SetConsoleOutputCP(65001);      /* 控制台 UTF-8 输出，防中文乱码 */

    int loaded = 0;
    g_head = fileio_load(DATA_FILE, &loaded);
    printf("已从 %s 加载 %d 条成绩记录\n", DATA_FILE, loaded);
    printf("已从 %s 加载 %d 个账号\n", USER_FILE, auth_load(USER_FILE));
    index_build(g_head);            /* 建立学号哈希索引 */

    /* 带参数 "web" 直接进页面模式（双击运行则询问） */
    if (argc > 1 && strcmp(argv[1], "web") == 0) {
        webui_start(&g_head, PORT_WEB);
    } else {
        printf("请选择运行模式:\n"
               "1. 浏览器页面模式(推荐，图形界面)\n"
               "2. 控制台菜单模式\n> ");
        char buf[8];
        if (fgets(buf, sizeof(buf), stdin) == NULL) buf[0] = '1';
        if (buf[0] == '2')
            console_mode();
        else
            webui_start(&g_head, PORT_WEB);
    }

    index_destroy();
    list_destroy(g_head);
    return 0;
}
