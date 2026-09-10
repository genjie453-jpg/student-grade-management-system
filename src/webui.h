#ifndef WEBUI_H
#define WEBUI_H

#include "list.h"

#define PORT_WEB 8080    /* 页面模式监听端口 */

/* 启动浏览器页面模式：在本机监听 port，并自动打开默认浏览器。
 * 阻塞运行（Ctrl+C 退出）；增删改会同步到 *ppHead 并自动保存文件。 */
int webui_start(Node** ppHead, int port);

#endif /* WEBUI_H */
