#ifndef API_DB_HANDLERS_H
#define API_DB_HANDLERS_H

#include "webserver/api/api_utils.h"
#include "webserver/web_core.h"
#include "webserver/web_parser.h"

void api_db_handle_files(socket_t client);
void api_db_handle_get(socket_t client, const HttpRequest *req);
void api_db_handle_add(socket_t client, const HttpRequest *req);
void api_db_handle_update(socket_t client, const HttpRequest *req);
void api_db_handle_delete(socket_t client, const HttpRequest *req);

#endif
