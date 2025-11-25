#ifndef API_COMPARE_HANDLERS_H
#define API_COMPARE_HANDLERS_H

#include "webserver/api/api_utils.h"
#include "webserver/web_core.h"
#include "webserver/web_parser.h"

void api_compare_handle_single(socket_t client, const HttpRequest *req);
void api_compare_handle_list(socket_t client, const HttpRequest *req);

#endif
