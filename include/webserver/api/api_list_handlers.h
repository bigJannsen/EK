#ifndef API_LIST_HANDLERS_H
#define API_LIST_HANDLERS_H

#include "webserver/api/api_utils.h"
#include "webserver/web_core.h"
#include "webserver/web_parser.h"

void api_list_handle_get(socket_t client);
void api_list_handle_add(socket_t client, const HttpRequest *req);
void api_list_handle_update(socket_t client, const HttpRequest *req);
void api_list_handle_delete(socket_t client, const HttpRequest *req);
void api_list_handle_download(socket_t client);

#endif
