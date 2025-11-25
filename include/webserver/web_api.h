#ifndef WEB_API_H
#define WEB_API_H

#include "webserver/web_core.h"
#include "webserver/web_parser.h"

void handle_db_files(socket_t client);
void handle_db_get(socket_t client, const HttpRequest *req);
void handle_db_add(socket_t client, const HttpRequest *req);
void handle_db_update(socket_t client, const HttpRequest *req);
void handle_db_delete(socket_t client, const HttpRequest *req);
void handle_list_get(socket_t client);
void handle_list_add(socket_t client, const HttpRequest *req);
void handle_list_update(socket_t client, const HttpRequest *req);
void handle_list_delete(socket_t client, const HttpRequest *req);
void handle_list_download(socket_t client);
void handle_compare_single(socket_t client, const HttpRequest *req);
void handle_compare_list(socket_t client, const HttpRequest *req);

#endif
