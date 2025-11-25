#ifndef WEB_STATIC_H
#define WEB_STATIC_H

#include <stddef.h>

#include "webserver/web_core.h"

int build_static_path(const char *relative, char *out, size_t out_size);
void send_file_response(socket_t client, const char *path);
void handle_static_root(socket_t client);
void handle_static_request(socket_t client, const char *relative);

#endif
