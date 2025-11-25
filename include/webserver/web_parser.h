#ifndef WEB_PARSER_H
#define WEB_PARSER_H

#include <stddef.h>

#include "webserver/web_core.h"

#define MAX_REQUEST_SIZE 65536
#define MAX_PARAMS 64
#define MAX_KEY_LEN 64
#define MAX_VALUE_LEN 512

typedef struct {
    char key[MAX_KEY_LEN];
    char value[MAX_VALUE_LEN];
} Param;

typedef struct {
    char method[8];
    char path[256];
    char query[512];
    char content_type[64];
    size_t content_length;
    Param query_params[MAX_PARAMS];
    int query_count;
    Param body_params[MAX_PARAMS];
    int body_count;
    char *body;
} HttpRequest;

int parse_http_request(char *buffer, size_t length, HttpRequest *req);
int read_http_request(socket_t client, char *buffer, size_t buffer_size, size_t *out_len);
const char *find_param(const Param *params, int count, const char *key);

#endif
