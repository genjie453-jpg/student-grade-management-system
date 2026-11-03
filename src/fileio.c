/* 文件持久化：加载 / 全量保存 / 追加
 *
 * ================== 数据安全设计（防"写到一半退出"丢数据） ==================
 * 1) 全量保存不再直接 fopen(path,"w")（那会在打开瞬间就把原文件清空）。
 *    改为"临时文件 + 落盘 + 原子改名"：
 *       写 <path>.<pid>.tmp → fflush → _commit（强制刷到磁盘）
 *       → MoveFileEx(tmp, path, MOVEFILE_REPLACE_EXISTING) 原子替换
 *    在替换成功的那一刻之前，原文件始终是完整的。所以无论进程被 Ctrl+C、
 *    任务管理器结束、崩溃还是断电，最坏结果只是"这次修改没保存成功"，
 *    绝不会出现文件被清空或只写了一半的情况。
 * 2) 替换时把上一版完整数据保留为 <path>.bak，误删/误改可手工恢复。
 * 3) 追加写入（录入）失败会回滚截断，避免留下半行记录。
 * 4) 加载改为按行解析：某一行损坏只跳过这一行并给出警告，不会像 fscanf
 *    那样把后面记录一起错位解析。
 * ============================================================================
 */
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <errno.h>
#include <io.h>
#include <fcntl.h>
#include <sys/stat.h>

#include "fileio.h"

#define FILEIO_PATH_MAX 512

/* 最近一次失败原因（供界面/控制台提示用） */
static char s_lastError[256] = "";

static void set_error(const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s_lastError, sizeof(s_lastError), fmt, ap);
    va_end(ap);
}

const char* fileio_last_error(void)
{
    return s_lastError;
}

/* 拼出配套文件路径，成功返回 1 */
static int make_peer_path(char* dst, size_t cap, const char* path, const char* suffix)
{
    int n = snprintf(dst, cap, "%s%s", path, suffix);
    return n > 0 && (size_t)n < cap;
}

static int make_tmp_path(char* dst, size_t cap, const char* path)
{
    int n = snprintf(dst, cap, "%s.%lu.tmp", path, (unsigned long)GetCurrentProcessId());
    return n > 0 && (size_t)n < cap;
}

/* 清理上次异常退出遗留的临时文件：只删除“写入它的进程已经不在了”的那些，
 * 因此不会误删另一个正在运行的实例的临时文件。 */
static void cleanup_stale_temps(const char* path)
{
    char pattern[FILEIO_PATH_MAX], dir[FILEIO_PATH_MAX];
    const char* name = path;
    const char* p;

    /* 拆出目录与文件名 */
    for (p = path; *p != '\0'; p++)
        if (*p == '\\' || *p == '/')
            name = p + 1;
    size_t dirLen = (size_t)(name - path);
    if (dirLen >= sizeof(dir))
        return;
    memcpy(dir, path, dirLen);
    dir[dirLen] = '\0';

    if (snprintf(pattern, sizeof(pattern), "%s.*.tmp", path) <= 0)
        return;

    WIN32_FIND_DATAA fd;
    HANDLE h = FindFirstFileA(pattern, &fd);
    if (h == INVALID_HANDLE_VALUE)
        return;

    do {
        const char* fname = fd.cFileName;
        size_t nlen = strlen(name);
        if (strncmp(fname, name, nlen) != 0 || fname[nlen] != '.')
            continue;

        /* 文件名形如 <name>.<pid>.tmp，取中间的 pid */
        const char* suffix = fname + nlen + 1;
        char* end = NULL;
        unsigned long pid = strtoul(suffix, &end, 10);
        if (end == NULL || strcmp(end, ".tmp") != 0 || pid == 0)
            continue;

        HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, (DWORD)pid);
        if (proc == NULL && GetLastError() == ERROR_INVALID_PARAMETER) {
            char full[FILEIO_PATH_MAX];        /* 进程已不存在，属于残留临时文件 */
            if ((size_t)snprintf(full, sizeof(full), "%s%s", dir, fname) < sizeof(full))
                DeleteFileA(full);
        } else if (proc != NULL) {
            CloseHandle(proc);
        }
    } while (FindNextFileA(h, &fd));

    FindClose(h);
}

/* ---------------- 加载 ---------------- */

/* 解析一行记录；成功返回 1。字段宽度与 Student 的数组长度对应 */
static int parse_student_line(const char* line, Student* s)
{
    int n = sscanf(line, "%15s %31s %31s %15s %31s %f",
                   s->id, s->name, s->className, s->term, s->course, &s->score);
    if (n != 6)
        return 0;
    /* 拒绝 nan/inf：它们会让后续比较、统计全部出错 */
    if (!(s->score > -1.0e9f && s->score < 1.0e9f))
        return 0;
    return 1;
}

/* 提示上一版数据备份的位置（数据文件丢失或有损坏行时给出） */
static void hint_backup(const char* path)
{
    char bak[FILEIO_PATH_MAX];
    if (!make_peer_path(bak, sizeof(bak), path, ".bak"))
        return;
    if (GetFileAttributesA(bak) == INVALID_FILE_ATTRIBUTES)
        return;
    /* 备份里没有内容就不用提示 */
    FILE* fp = fopen(bak, "r");
    if (fp == NULL)
        return;
    int hasData = (fgetc(fp) != EOF);
    fclose(fp);
    if (hasData)
        fprintf(stderr,
                "[提示] 上一版完整数据备份在 %s；若发现记录缺失，"
                "把该备份改名覆盖数据文件即可恢复。\n", bak);
}

Node* fileio_load(const char* path, int* outCount)
{
    if (outCount != NULL)
        *outCount = 0;

    if (path == NULL) {
        set_error("文件路径为空");
        return NULL;
    }

    /* 顺手清掉上次崩溃遗留的临时文件 */
    cleanup_stale_temps(path);

    FILE* fp = fopen(path, "r");
    if (fp == NULL) {
        /* 首次运行文件不存在，属正常情况 */
        if (errno == ENOENT)
            hint_backup(path);
        else
            set_error("无法打开 %s（errno=%d）", path, errno);
        return NULL;
    }

    Node* head = NULL;
    Student s;
    int count = 0, bad = 0, firstBadLine = 0, lineNo = 0, first = 1;
    char line[MAX_LINE_LEN];

    while (fgets(line, (int)sizeof(line), fp) != NULL) {
        lineNo++;
        size_t len = strlen(line);

        /* 超长行（没读到换行且不是文件末尾）：丢掉剩余部分，避免拆成两行误解析 */
        if (len > 0 && line[len - 1] != '\n' && !feof(fp)) {
            int c;
            while ((c = fgetc(fp)) != '\n' && c != EOF) { }
            if (bad == 0) firstBadLine = lineNo;
            bad++;
            continue;
        }

        /* 去掉行尾 CR/LF，便于处理 Windows 换行与被手工编辑过的文件 */
        while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r'))
            line[--len] = '\0';

        /* 跳过 UTF-8 BOM（记事本保存的文件常带） */
        if (first && len >= 3 &&
            (unsigned char)line[0] == 0xEF &&
            (unsigned char)line[1] == 0xBB &&
            (unsigned char)line[2] == 0xBF) {
            memmove(line, line + 3, len - 3 + 1);
            len -= 3;
        }
        first = 0;

        if (len == 0 || line[0] == '#')     /* 空行、注释行 */
            continue;

        memset(&s, 0, sizeof(s));
        if (!parse_student_line(line, &s)) {
            if (bad == 0) firstBadLine = lineNo;
            bad++;
            continue;
        }

        head = list_insert_tail(head, s);   /* 尾插保持文件顺序 */
        count++;
    }
    fclose(fp);

    if (bad > 0) {
        fprintf(stderr,
                "[警告] %s 有 %d 行格式不正确（首个在第 %d 行），已跳过，其余记录正常加载。\n",
                path, bad, firstBadLine);
        set_error("%d 行格式不正确（首个在第 %d 行）", bad, firstBadLine);
        hint_backup(path);
    }

    if (outCount != NULL)
        *outCount = count;
    return head;
}

/* ---------------- 原子替换 ---------------- */

/* 用一个单独的改名把 tmp 换成 path，并把旧版本先复制成 bak。成功返回 1。
 *
 * 关键：替换只用一次 MoveFileEx（NTFS 上是原子的 rename-with-replace），
 * 绝不能用 ReplaceFileA —— 它内部是"先改名目标、再改名新文件"的多步操作，
 * 进程恰好在这两步之间被杀时，目标文件会短暂不存在（实测可复现）。
 * 备份用复制而不是改名，所以整个过程中 path 始终存在，且要么是旧版、要么是新版。 */
static int replace_file(const char* path, const char* tmp, const char* bak)
{
    char bakTmp[FILEIO_PATH_MAX];

    /* 1) 目标已存在时，先把旧版本完整复制为 .bak（复制失败不影响保存本身） */
    if (GetFileAttributesA(path) != INVALID_FILE_ATTRIBUTES &&
        make_peer_path(bakTmp, sizeof(bakTmp), path, ".bak.tmp")) {
        if (CopyFileA(path, bakTmp, FALSE))
            MoveFileExA(bakTmp, bak, MOVEFILE_REPLACE_EXISTING);
        else
            DeleteFileA(bakTmp);
    }

    /* 2) 单次原子改名完成替换；失败时 path 保持原样 */
    return MoveFileExA(tmp, path,
                       MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

/* ---------------- 保存 ---------------- */

int fileio_save(const char* path, Node* head)
{
    char tmp[FILEIO_PATH_MAX], bak[FILEIO_PATH_MAX];

    if (path == NULL) {
        set_error("文件路径为空");
        return -1;
    }
    if (!make_tmp_path(tmp, sizeof(tmp), path) ||
        !make_peer_path(bak, sizeof(bak), path, ".bak")) {
        set_error("路径过长");
        return -1;
    }

    /* 先完整写到临时文件；此阶段无论怎么中断，原文件都不受影响 */
    FILE* fp = fopen(tmp, "wb");
    if (fp == NULL) {
        set_error("无法创建临时文件 %s（errno=%d）", tmp, errno);
        return -1;
    }

    int count = 0, ok = 1;
    for (Node* p = head; p != NULL; p = p->next) {
        if (fprintf(fp, "%s %s %s %s %s %.1f\n",
                    p->data.id, p->data.name, p->data.className,
                    p->data.term, p->data.course, p->data.score) < 0) {
            ok = 0;
            break;
        }
        count++;
    }
    if (ok && fflush(fp) != 0)
        ok = 0;
    if (ok && _commit(_fileno(fp)) != 0)   /* 强制刷到磁盘，防"改完就断电" */
        ok = 0;
    if (ferror(fp))
        ok = 0;
    if (fclose(fp) != 0)
        ok = 0;

    if (!ok) {
        set_error("写入临时文件失败（errno=%d），原文件未改动", errno);
        DeleteFileA(tmp);
        return -1;
    }

    if (!replace_file(path, tmp, bak)) {
        DWORD err = GetLastError();
        set_error("替换 %s 失败（Win32 错误码 %lu），原文件未改动",
                  path, (unsigned long)err);
        DeleteFileA(tmp);
        return -1;
    }

    set_error("");
    return count;
}

/* ---------------- 追加 ---------------- */

/* 把文件截断回指定长度（写失败时回滚掉半行记录） */
static void truncate_to(const char* path, long long size)
{
    FILE* fp = fopen(path, "r+b");
    if (fp == NULL)
        return;
    _chsize_s(_fileno(fp), size);
    fclose(fp);
}

int fileio_append(const char* path, const Student* s)
{
    if (path == NULL || s == NULL) {
        set_error("参数为空");
        return 0;
    }

    FILE* fp = fopen(path, "ab");
    if (fp == NULL) {
        set_error("无法打开 %s（errno=%d）", path, errno);
        return 0;
    }

    int fd = _fileno(fp);
    long long oldSize = _filelengthi64(fd);   /* 记下原长度，失败时回滚 */
    if (oldSize < 0)
        oldSize = 0;

    int ok = fprintf(fp, "%s %s %s %s %s %.1f\n",
                     s->id, s->name, s->className,
                     s->term, s->course, s->score) >= 0;
    if (ok && fflush(fp) != 0)
        ok = 0;
    if (ok && _commit(fd) != 0)
        ok = 0;
    if (ferror(fp))
        ok = 0;
    if (fclose(fp) != 0)
        ok = 0;

    if (!ok) {
        set_error("追加写入失败（errno=%d），已回滚", errno);
        truncate_to(path, oldSize);
        return 0;
    }

    set_error("");
    return 1;
}