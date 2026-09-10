#ifndef AUTH_H
#define AUTH_H

#define MAX_ACCOUNT 32
#define MAX_PWD     32
#define TOKEN_LEN   33      /* 32个十六进制字符 + '\0' */

#define USER_FILE   "data/users.txt"   /* 账号文件路径 */

/* 登录后的会话：令牌 -> 身份 */
typedef struct Session {
    char token[TOKEN_LEN];
    int  role;              /* 0管理员 1教师 2学生 */
    char id[MAX_ACCOUNT];   /* 学生=学号；教师/管理员=账号 */
    char name[MAX_ACCOUNT];
    struct Session* next;   /* 会话链表指针（服务端保存已登录用户） */
} Session;

/* 从 data/users.txt 加载账号（一行一条：账号 密码 角色 姓名）
 * 文件不存在时自动创建含默认账号的文件；返回加载的账号数 */
int auth_load(const char* path);

/* 登录验证：成功填充 *out 并返回1，失败返回0 */
int auth_login(const char* account, const char* password, Session* out);

/* 校验令牌：有效返回1并填充 *out，无效返回0 */
int auth_verify(const char* token, Session* out);

/* 注销：删除令牌 */
void auth_logout(const char* token);

#endif /* AUTH_H */
