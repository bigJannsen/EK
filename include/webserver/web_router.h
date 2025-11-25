#ifndef WEB_ROUTER_H
#define WEB_ROUTER_H

#include "webserver/web_core.h"
#include "webserver/web_parser.h"

void route_request(socket_t client, const HttpRequest *req);

#endif
