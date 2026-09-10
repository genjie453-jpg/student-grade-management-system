/* 浏览器页面模式：内置轻量 HTTP 服务器（Winsock）
 * 页面通过 fetch 调用 /api/* 接口，数据与链表实时同步 */
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "student.h"
#include "list.h"
#include "fileio.h"
#include "webui.h"

#pragma comment(lib, "ws2_32.lib")   /* MSVC 生效；gcc 用 -lws2_32 链接 */

#define PORT_DEFAULT 8080

/* ---------------- URL 解码（处理 %XX 与 +） ---------------- */
static void url_decode(char* dst, const char* src, int dstSize)
{
    int di = 0;
    for (int i = 0; src[i] != '\0' && di < dstSize - 1; i++) {
        if (src[i] == '+') {
            dst[di++] = ' ';
        } else if (src[i] == '%' && src[i+1] != '\0' && src[i+2] != '\0') {
            char hex[3] = { src[i+1], src[i+2], '\0' };
            dst[di++] = (char)strtol(hex, NULL, 16);
            i += 2;
        } else {
            dst[di++] = src[i];
        }
    }
    dst[di] = '\0';
}

/* ---------------- 查询参数 ---------------- */
typedef struct { char key[24]; char val[128]; } Param;

static int parse_query(char* query, Param* ps, int maxParams)
{
    int n = 0;
    char* save = NULL;
    for (char* tok = strtok_s(query, "&", &save); tok != NULL && n < maxParams;
         tok = strtok_s(NULL, "&", &save)) {
        char* eq = strchr(tok, '=');
        if (eq == NULL) continue;
        *eq = '\0';
        strncpy(ps[n].key, tok, sizeof(ps[n].key) - 1);
        ps[n].key[sizeof(ps[n].key) - 1] = '\0';
        url_decode(ps[n].val, eq + 1, (int)sizeof(ps[n].val));
        n++;
    }
    return n;
}

static const char* get_param(Param* ps, int n, const char* key)
{
    for (int i = 0; i < n; i++)
        if (strcmp(ps[i].key, key) == 0)
            return ps[i].val;
    return "";
}

/* ---------------- JSON 输出缓冲 ---------------- */
static char s_body[1 << 20];    /* 1MB 响应缓冲，够课程设计用了 */

static void json_str(char** pp, const char* s)
{
    *pp += sprintf(*pp, "\"%s\"", s);
}

/* 通用匹配：一条记录是否满足筛选条件（空条件视为不筛） */
static int match(Node* p, const char* id, const char* name,
                 const char* className, const char* term, const char* course)
{
    if (id[0]        != '\0' && strcmp(p->data.id, id) != 0) return 0;
    if (name[0]      != '\0' && strcmp(p->data.name, name) != 0) return 0;
    if (className[0] != '\0' && strcmp(p->data.className, className) != 0) return 0;
    if (term[0]      != '\0' && strcmp(p->data.term, term) != 0) return 0;
    if (course[0]    != '\0' && strcmp(p->data.course, course) != 0) return 0;
    return 1;
}

/* ---------------- API 处理 ---------------- */

/* /api/list ：查询（支持多条件筛选） */
static void api_list(Node* head, Param* ps, int np)
{
    const char* id        = get_param(ps, np, "id");
    const char* name      = get_param(ps, np, "name");
    const char* className = get_param(ps, np, "className");
    const char* term      = get_param(ps, np, "term");
    const char* course    = get_param(ps, np, "course");

    char* w = s_body;
    w += sprintf(w, "{\"code\":0,\"total\":");
    int total = 0;
    for (Node* p = head; p != NULL; p = p->next)
        if (match(p, id, name, className, term, course))
            total++;
    w += sprintf(w, "%d,\"data\":[", total);

    int first = 1;
    for (Node* p = head; p != NULL; p = p->next) {
        if (!match(p, id, name, className, term, course)) continue;
        if (!first) *w++ = ',';
        first = 0;
        w += sprintf(w, "{\"id\":");
        json_str(&w, p->data.id);
        w += sprintf(w, ",\"name\":");
        json_str(&w, p->data.name);
        w += sprintf(w, ",\"className\":");
        json_str(&w, p->data.className);
        w += sprintf(w, ",\"term\":");
        json_str(&w, p->data.term);
        w += sprintf(w, ",\"course\":");
        json_str(&w, p->data.course);
        w += sprintf(w, ",\"score\":%.1f}", p->data.score);
    }
    sprintf(w, "]}");
}

/* /api/add ：录入一条（防重复：同学号+学期+课程） */
static void api_add(Node** ppHead, Param* ps, int np)
{
    Student s = {0};
    strncpy(s.id,        get_param(ps, np, "id"),        MAX_ID_LEN - 1);
    strncpy(s.name,      get_param(ps, np, "name"),      MAX_NAME_LEN - 1);
    strncpy(s.className, get_param(ps, np, "className"), MAX_CLASS_LEN - 1);
    strncpy(s.term,      get_param(ps, np, "term"),      MAX_TERM_LEN - 1);
    strncpy(s.course,    get_param(ps, np, "course"),    MAX_COURSE_LEN - 1);
    s.score = (float)atof(get_param(ps, np, "score"));

    if (s.id[0] == '\0' || s.course[0] == '\0' || s.term[0] == '\0') {
        sprintf(s_body, "{\"code\":1,\"msg\":\"学号/学期/课程不能为空\"}");
        return;
    }
    if (s.score < 0 || s.score > 100) {
        sprintf(s_body, "{\"code\":1,\"msg\":\"成绩必须在0~100之间\"}");
        return;
    }
    if (list_exists(*ppHead, s.id, s.term, s.course)) {
        sprintf(s_body, "{\"code\":1,\"msg\":\"该学号在本学期的这门课已存在\"}");
        return;
    }
    *ppHead = list_insert_tail(*ppHead, s);
    fileio_append(DATA_FILE, &s);
    sprintf(s_body, "{\"code\":0,\"msg\":\"录入成功\"}");
}

/* /api/update ：修改成绩 */
static void api_update(Node** ppHead, Param* ps, int np)
{
    const char* id     = get_param(ps, np, "id");
    const char* term   = get_param(ps, np, "term");
    const char* course = get_param(ps, np, "course");
    float score = (float)atof(get_param(ps, np, "score"));

    if (score < 0 || score > 100) {
        sprintf(s_body, "{\"code\":1,\"msg\":\"成绩必须在0~100之间\"}");
        return;
    }
    if (list_update_score(*ppHead, id, term, course, score)) {
        fileio_save(DATA_FILE, *ppHead);
        sprintf(s_body, "{\"code\":0,\"msg\":\"修改成功\"}");
    } else {
        sprintf(s_body, "{\"code\":1,\"msg\":\"未找到匹配的记录\"}");
    }
}

/* /api/delete ：删除记录 */
static void api_delete(Node** ppHead, Param* ps, int np)
{
    int deleted = 0;
    *ppHead = list_delete_one(*ppHead, get_param(ps, np, "id"),
                              get_param(ps, np, "term"),
                              get_param(ps, np, "course"), &deleted);
    if (deleted) {
        fileio_save(DATA_FILE, *ppHead);
        sprintf(s_body, "{\"code\":0,\"msg\":\"删除成功\"}");
    } else {
        sprintf(s_body, "{\"code\":1,\"msg\":\"未找到匹配的记录\"}");
    }
}

/* 排名比较：分数降序 */
static int cmp_score_desc(const void* a, const void* b)
{
    float fa = ((const Student*)a)->score;
    float fb = ((const Student*)b)->score;
    return (fb > fa) - (fb < fa);
}

/* /api/stats ：统计分析（平均分/及格率/排名），空 course 表示对当前筛选集合统计 */
static void api_stats(Node* head, Param* ps, int np)
{
    const char* id        = get_param(ps, np, "id");
    const char* name      = get_param(ps, np, "name");
    const char* className = get_param(ps, np, "className");
    const char* term      = get_param(ps, np, "term");
    const char* course    = get_param(ps, np, "course");

    static Student arr[2048];       /* 课程设计规模足够 */
    int n = 0;
    for (Node* p = head; p != NULL && n < 2048; p = p->next)
        if (match(p, id, name, className, term, course))
            arr[n++] = p->data;

    double sum = 0.0;
    int pass = 0;
    for (int i = 0; i < n; i++) {
        sum += arr[i].score;
        if (arr[i].score >= PASS_SCORE)
            pass++;
    }

    qsort(arr, (size_t)n, sizeof(Student), cmp_score_desc);

    char* w = s_body;
    w += sprintf(w, "{\"code\":0,\"count\":%d,\"avg\":%.2f,\"pass\":%d,\"passRate\":%.1f,\"data\":[",
                 n, n > 0 ? sum / n : 0.0, pass, n > 0 ? pass * 100.0 / n : 0.0);
    for (int i = 0; i < n; i++) {
        /* 并列排名：同分同名次，名次 = 前面比它高的人数 + 1 */
        int rank = 1;
        for (int j = 0; j < i; j++)
            if (arr[j].score > arr[i].score)
                rank++;
        if (i) *w++ = ',';
        w += sprintf(w, "{\"rank\":%d,\"id\":", rank);
        json_str(&w, arr[i].id);
        w += sprintf(w, ",\"name\":");
        json_str(&w, arr[i].name);
        w += sprintf(w, ",\"className\":");
        json_str(&w, arr[i].className);
        w += sprintf(w, ",\"term\":");
        json_str(&w, arr[i].term);
        w += sprintf(w, ",\"course\":");
        json_str(&w, arr[i].course);
        w += sprintf(w, ",\"score\":%.1f}", arr[i].score);
    }
    sprintf(w, "]}");
}

/* /api/save ：手动全量保存 */
static void api_save(Node* head)
{
    int n = fileio_save(DATA_FILE, head);
    sprintf(s_body, "{\"code\":%d,\"msg\":\"已保存 %d 条到 %s\"}",
            n >= 0 ? 0 : 1, n, DATA_FILE);
}

/* ---------------- 页面 HTML（内嵌，浏览器直接渲染） ---------------- */
static const char* PAGE_HTML =
"<!DOCTYPE html><html lang=\"zh\"><head><meta charset=\"utf-8\">"
"<meta name=\"viewport\" content=\"width=device-width,initial-scale=1\">"
"<title>学生成绩管理系统</title><style>"
"body{font-family:'Microsoft YaHei',sans-serif;margin:0;background:#f0f2f5;color:#333}"
".top{background:#1f3a5f;color:#fff;padding:14px 24px;font-size:20px;font-weight:bold}"
".wrap{max-width:1100px;margin:20px auto;padding:0 16px}"
".card{background:#fff;border-radius:8px;padding:16px 20px;margin-bottom:16px;box-shadow:0 1px 4px rgba(0,0,0,.08)}"
"input,select{padding:6px 10px;border:1px solid #ccc;border-radius:4px;margin:4px 6px 4px 0}"
"button{padding:6px 16px;border:none;border-radius:4px;background:#2f6fdb;color:#fff;cursor:pointer;margin:4px 4px 4px 0}"
"button.red{background:#d9534f}button.gray{background:#8a97a6}"
"table{width:100%;border-collapse:collapse;margin-top:10px;font-size:14px}"
"th,td{border-bottom:1px solid #eee;padding:8px 6px;text-align:center}"
"th{background:#f5f7fa}"
".score-hi{color:#2e9e4f;font-weight:bold}.score-lo{color:#d9534f;font-weight:bold}"
".msg{margin-top:8px;color:#2f6fdb;min-height:20px}"
".stat span{display:inline-block;margin-right:18px;font-size:16px}"
".bar{background:#4da3ff;height:12px;border-radius:6px}"
".rank1{color:#d4a017;font-weight:bold}"
"</style></head><body>"
"<div class=\"top\">📊 学生成绩管理系统 - 数据结构课程设计</div><div class=\"wrap\">"

"<div class=\"card\"><b>角色</b>：<select id=\"role\" onchange=\"roleChange()\">"
"<option value=\"admin\">管理员</option><option value=\"teacher\">教师</option>"
"<option value=\"student\">学生</option></select>"
"<span id=\"stuBox\" style=\"display:none\">学号：<input id=\"stuId\" placeholder=\"输入本人学号\">"
"<button onclick=\"viewSelf()\">查询我的成绩</button></span></div>"

"<div class=\"card\" id=\"addCard\"><b>➕ 录入成绩</b><br>"
"<input id=\"a_id\" placeholder=\"学号\"><input id=\"a_name\" placeholder=\"姓名\">"
"<input id=\"a_class\" placeholder=\"班级 如计科2301\"><input id=\"a_term\" placeholder=\"学期 如2025-2026-1\">"
"<input id=\"a_course\" placeholder=\"课程\"><input id=\"a_score\" type=\"number\" min=\"0\" max=\"100\" placeholder=\"分数\">"
"<button onclick=\"addRec()\">提交</button></div>"

"<div class=\"card\"><b>🔎 查询</b>（条件可留空）<br>"
"<input id=\"f_id\" placeholder=\"学号\"><input id=\"f_name\" placeholder=\"姓名\">"
"<input id=\"f_class\" placeholder=\"班级\"><input id=\"f_term\" placeholder=\"学期\">"
"<input id=\"f_course\" placeholder=\"课程\">"
"<button onclick=\"loadList()\">查询</button>"
"<button class=\"gray\" onclick=\"clearFilter()\">重置</button>"
"<button class=\"gray\" onclick=\"saveAll()\">💾 保存到文件</button>"
"<div class=\"msg\" id=\"msg\"></div>"
"<table><thead><tr><th>学号</th><th>姓名</th><th>班级</th><th>学期</th><th>课程</th><th>成绩</th><th>操作</th></tr></thead>"
"<tbody id=\"tb\"></tbody></table></div>"

"<div class=\"card\"><b>📈 统计分析</b>（按课程+学期）<br>"
"<input id=\"s_course\" placeholder=\"课程（留空=全部）\"><input id=\"s_term\" placeholder=\"学期（留空=全部）\">"
"<button onclick=\"loadStats()\">统计</button>"
"<div class=\"stat msg\" id=\"statMsg\"></div>"
"<table><thead><tr><th>排名</th><th>学号</th><th>姓名</th><th>班级</th><th>课程</th><th>成绩</th><th>分布</th></tr></thead>"
"<tbody id=\"stb\"></tbody></table></div>"

"</div><script>"
"const $=id=>document.getElementById(id);"
"function roleChange(){const r=$('role').value;"
"$('addCard').style.display=(r==='student')?'none':'block';"
"$('stuBox').style.display=(r==='student')?'inline':'none';"
"if(r==='student')loadList();}"
"function msg(t){$('msg').textContent=t;setTimeout(()=>{$('msg').textContent=''},3000);}"
"async function api(url){const r=await fetch(url);return r.json();}"
"function tdScore(s){return '<td class=\"'+(s>=60?'score-hi':'score-lo')+'\">'+s+'</td>';}"
"async function loadList(){"
"const q=new URLSearchParams({id:$('f_id').value,name:$('f_name').value,className:$('f_class').value,term:$('f_term').value,course:$('f_course').value});"
"const d=await api('/api/list?'+q);const admin=$('role').value!=='student';"
"$('tb').innerHTML=d.data.map(r=>'<tr><td>'+r.id+'</td><td>'+r.name+'</td><td>'+r.className+'</td><td>'+r.term+'</td><td>'+r.course+'</td>'+tdScore(r.score)"
"+(admin?'<td><button class=\\'gray\\' onclick=\\'upd(\\\"'+r.id+'\\\",\\\"'+r.term+'\\\",\\\"'+r.course+'\\\",'+r.score+')\\'>改</button> '"
"+'<button class=\\'red\\' onclick=\\'del(\\\"'+r.id+'\\\",\\\"'+r.term+'\\\",\\\"'+r.course+'\\\")\\'>删</button></td>':'<td>-</td>')"
"+'</tr>').join('')||'<tr><td colspan=7>无记录</td></tr>';}"
"function clearFilter(){['f_id','f_name','f_class','f_term','f_course'].forEach(i=>$(i).value='');loadList();}"
"async function addRec(){"
"const q=new URLSearchParams({id:$('a_id').value,name:$('a_name').value,className:$('a_class').value,term:$('a_term').value,course:$('a_course').value,score:$('a_score').value});"
"const d=await api('/api/add?'+q);msg(d.msg);if(d.code===0){loadList();['a_id','a_name','a_course','a_score'].forEach(i=>$(i).value='');}}"
"async function upd(id,term,cur,score){const v=prompt('修改 '+id+' 的「'+cur+'」成绩：',score);"
"if(v===null)return;const d=await api('/api/update?id='+id+'&term='+encodeURIComponent(term)+'&course='+encodeURIComponent(cur)+'&score='+v);msg(d.msg);loadList();}"
"async function del(id,term,course){if(!confirm('确定删除 '+id+' 的「'+course+'」记录？'))return;"
"const d=await api('/api/delete?id='+id+'&term='+encodeURIComponent(term)+'&course='+encodeURIComponent(course));msg(d.msg);loadList();}"
"async function saveAll(){const d=await api('/api/save');msg(d.msg);}"
"async function viewSelf(){$('f_id').value=$('stuId').value;['f_name','f_class','f_term','f_course'].forEach(i=>$(i).value='');loadList();}"
"async function loadStats(){"
"const q=new URLSearchParams({course:$('s_course').value,term:$('s_term').value,id:$('role').value==='student'?$('stuId').value:''});"
"const d=await api('/api/stats?'+q);"
"$('statMsg').innerHTML='<span>人数：<b>'+d.count+'</b></span><span>平均分：<b>'+(d.count?d.avg:'-')+'</b></span>"
"<span>及格：<b>'+d.pass+'</b></span><span>及格率：<b>'+(d.count?d.passRate+'%':'-')+'</b></span>';"
"$('stb').innerHTML=d.data.map(r=>'<tr><td class=\"'+(r.rank===1?'rank1':'')+'\">'+(r.rank===1?'🥇 ':r.rank)+'</td><td>'+r.id+'</td><td>'+r.name+'</td><td>'+r.className+'</td><td>'+r.course+'</td>'+tdScore(r.score)"
"+'<td><div class=\"bar\" style=\"width:'+r.score+'%\"></div></td></tr>').join('')||'<tr><td colspan=7>无数据</td></tr>';}"
"roleChange();loadList();"
"</script></body></html>";

/* ---------------- HTTP 应答 ---------------- */

static void send_all(SOCKET sock, const char* buf, int len)
{
    int sent = 0;
    while (sent < len) {
        int n = send(sock, buf + sent, len - sent, 0);
        if (n <= 0) break;
        sent += n;
    }
}

static void respond(SOCKET sock, const char* contentType, const char* body)
{
    char header[256];
    int bodyLen = (int)strlen(body);
    int h = sprintf(header,
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s; charset=utf-8\r\n"
        "Content-Length: %d\r\n"
        "Connection: close\r\n\r\n", contentType, bodyLen);
    send_all(sock, header, h);
    send_all(sock, body, bodyLen);
}

/* 根据 URL 路由到对应处理函数 */
static void handle(SOCKET client, Node** ppHead, char* pathWithQuery)
{
    char* query = strchr(pathWithQuery, '?');
    if (query != NULL)
        *query++ = '\0';

    Param ps[16];
    int np = 0;
    if (query != NULL && query[0] != '\0')
        np = parse_query(query, ps, 16);

    if (strcmp(pathWithQuery, "/") == 0 || strcmp(pathWithQuery, "/index.html") == 0) {
        respond(client, "text/html", PAGE_HTML);
    } else if (strcmp(pathWithQuery, "/api/list") == 0) {
        api_list(*ppHead, ps, np);
        respond(client, "application/json", s_body);
    } else if (strcmp(pathWithQuery, "/api/add") == 0) {
        api_add(ppHead, ps, np);
        respond(client, "application/json", s_body);
    } else if (strcmp(pathWithQuery, "/api/update") == 0) {
        api_update(ppHead, ps, np);
        respond(client, "application/json", s_body);
    } else if (strcmp(pathWithQuery, "/api/delete") == 0) {
        api_delete(ppHead, ps, np);
        respond(client, "application/json", s_body);
    } else if (strcmp(pathWithQuery, "/api/stats") == 0) {
        api_stats(*ppHead, ps, np);
        respond(client, "application/json", s_body);
    } else if (strcmp(pathWithQuery, "/api/save") == 0) {
        api_save(*ppHead);
        respond(client, "application/json", s_body);
    } else {
        respond(client, "text/plain", "404 Not Found");
    }
}

int webui_start(Node** ppHead, int port)
{
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("[错误] WSAStartup 失败\n");
        return -1;
    }

    SOCKET listenSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSock == INVALID_SOCKET) {
        printf("[错误] 创建套接字失败\n");
        WSACleanup();
        return -1;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons((u_short)port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);   /* 只允许本机访问 */

    if (bind(listenSock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        printf("[错误] 端口 %d 被占用，请关闭占用程序后重试\n", port);
        closesocket(listenSock);
        WSACleanup();
        return -1;
    }
    if (listen(listenSock, 8) == SOCKET_ERROR) {
        printf("[错误] listen 失败\n");
        closesocket(listenSock);
        WSACleanup();
        return -1;
    }

    /* 自动打开浏览器 */
    char url[64];
    sprintf(url, "http://127.0.0.1:%d", port);
    ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
    printf("页面模式已启动：%s （关闭本窗口或按 Ctrl+C 退出）\n", url);

    char req[8192];
    for (;;) {
        SOCKET client = accept(listenSock, NULL, NULL);
        if (client == INVALID_SOCKET)
            continue;

        int len = recv(client, req, sizeof(req) - 1, 0);
        if (len > 0) {
            req[len] = '\0';
            /* 解析请求行：GET /path HTTP/1.1 */
            char method[8] = {0}, target[2048] = {0};
            if (sscanf(req, "%7s %2047s", method, target) == 2
                && strcmp(method, "GET") == 0) {
                handle(client, ppHead, target);
            } else {
                respond(client, "text/plain", "Method Not Allowed");
            }
        }
        closesocket(client);
    }
}
