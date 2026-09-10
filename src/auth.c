#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include "auth.h"

/* ---------------- 账号表（链表） ---------------- */

typedef struct User {
    char account[MAX_ACCOUNT];
    char pwd[MAX_PWD];
    int  role;
    char name[MAX_ACCOUNT];
    struct User* next;
} User;

static User* s_users = NULL;
static Session* s_sessions = NULL;      /* 已登录的会话链 */

int auth_load(const char* path)
{
    /* 文件不存在：生成默认账号文件后重新打开 */
    FILE* test = fopen(path, "r");
    if (test == NULL) {
        FILE* fp = fopen(path, "w");
        if (fp == NULL) return 0;
        fprintf(fp, "admin admin123 0 管理员\n");
        fprintf(fp, "teacher01 123456 1 王老师\n");
        fprintf(fp, "230101 123456 2 张三\n");
        fprintf(fp, "230102 123456 2 李四\n");
        fprintf(fp, "230103 123456 2 王五\n");
        fprintf(fp, "230104 123456 2 赵六\n");
        fprintf(fp, "230105 123456 2 钱七\n");
        fclose(fp);
    } else {
        fclose(test);
    }

    /* 清空旧表（支持重载） */
    while (s_users != NULL) {
        User* n = s_users->next;
        free(s_users);
        s_users = n;
    }

    FILE* fp = fopen(path, "r");
    if (fp == NULL) return 0;

    char account[MAX_ACCOUNT], pwd[MAX_PWD], name[MAX_ACCOUNT];
    int role, count = 0;
    while (fscanf(fp, "%31s %31s %d %31s", account, pwd, &role, name) == 4) {
        User* u = (User*)malloc(sizeof(User));
        if (u == NULL) break;
        strncpy(u->account, account, MAX_ACCOUNT - 1); u->account[MAX_ACCOUNT-1] = '\0';
        strncpy(u->pwd, pwd, MAX_PWD - 1);             u->pwd[MAX_PWD-1] = '\0';
        strncpy(u->name, name, MAX_ACCOUNT - 1);       u->name[MAX_ACCOUNT-1] = '\0';
        u->role = (role >= 0 && role <= 2) ? role : 2;
        u->next = s_users;
        s_users = u;
        count++;
    }
    fclose(fp);
    srand((unsigned)time(NULL));
    return count;
}

/* 生成 32 位十六进制随机令牌 */
static void gen_token(char* out)
{
    static const char hex[] = "0123456789abcdef";
    for (int i = 0; i < TOKEN_LEN - 1; i++)
        out[i] = hex[rand() % 16];
    out[TOKEN_LEN - 1] = '\0';
}

int auth_login(const char* account, const char* password, Session* out)
{
    if (account == NULL || password == NULL)
        return 0;

    for (User* u = s_users; u != NULL; u = u->next) {
        if (strcmp(u->account, account) == 0 && strcmp(u->pwd, password) == 0) {
            memset(out, 0, sizeof(Session));
            gen_token(out->token);
            out->role = u->role;
            strncpy(out->id, u->account, MAX_ACCOUNT - 1);
            strncpy(out->name, u->name, MAX_ACCOUNT - 1);

            Session* s = (Session*)malloc(sizeof(Session));
            if (s != NULL) {
                *s = *out;
                s->next = s_sessions;
                s_sessions = s;
            }
            return 1;
        }
    }
    return 0;
}

int auth_verify(const char* token, Session* out)
{
    if (token == NULL || token[0] == '\0')
        return 0;
    for (Session* s = s_sessions; s != NULL; s = s->next) {
        if (strcmp(s->token, token) == 0) {
            *out = *s;
            return 1;
        }
    }
    return 0;
}

void auth_logout(const char* token)
{
    Session *prev = NULL, *cur = s_sessions;
    while (cur != NULL) {
        if (strcmp(cur->token, token) == 0) {
            if (prev == NULL) s_sessions = cur->next;
            else prev->next = cur->next;
            free(cur);
            return;
        }
        prev = cur;
        cur = cur->next;
    }
}
