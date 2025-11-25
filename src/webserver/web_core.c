#include "webserver/web_core.h"

#include "config.h"
#include "webserver/web_parser.h"
#include "webserver/web_router.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int buffer_reserve(Buffer *buf, size_t needed) {
    if (buf->len + needed < buf->cap) {
        return 0;
    }
    size_t new_cap = buf->cap;
    while (new_cap < buf->len + needed + 1) {
        new_cap *= 2;
    }
    char *new_data = (char *)realloc(buf->data, new_cap);
    if (new_data == NULL) {
        return -1;
    }
    buf->data = new_data;
    buf->cap = new_cap;
    return 0;
}

int buffer_init(Buffer *buf) {
    buf->data = (char *)malloc(1024);
    if (buf->data == NULL) {
        return -1;
    }
    buf->len = 0;
    buf->cap = 1024;
    buf->data[0] = '\0';
    return 0;
}

int buffer_append(Buffer *buf, const char *data, size_t data_len) {
    if (buffer_reserve(buf, data_len) != 0) {
        return -1;
    }
    memcpy(buf->data + buf->len, data, data_len);
    buf->len += data_len;
    buf->data[buf->len] = '\0';
    return 0;
}

int buffer_append_str(Buffer *buf, const char *text) {
    return buffer_append(buf, text, strlen(text));
}

int buffer_append_char(Buffer *buf, char c) {
    return buffer_append(buf, &c, 1);
}

int buffer_append_format(Buffer *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char temp[512];
    int written = vsnprintf(temp, sizeof temp, fmt, args);
    va_end(args);
    if (written < 0) {
        return -1;
    }
    if ((size_t)written >= sizeof temp) {
        size_t needed = (size_t)written + 1;
        char *dynamic = (char *)malloc(needed);
        if (dynamic == NULL) {
            return -1;
        }
        va_start(args, fmt);
        vsnprintf(dynamic, needed, fmt, args);
        va_end(args);
        int res = buffer_append(buf, dynamic, (size_t)written);
        free(dynamic);
        return res;
    }
    return buffer_append(buf, temp, (size_t)written);
}

void buffer_free(Buffer *buf) {
    free(buf->data);
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

void append_json_string(Buffer *buf, const char *text) {
    buffer_append_char(buf, '"');
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        unsigned char c = *p;
        switch (c) {
            case '\\': buffer_append_str(buf, "\\\\"); break;
            case '"': buffer_append_str(buf, "\\\""); break;
            case '\n': buffer_append_str(buf, "\\n"); break;
            case '\r': buffer_append_str(buf, "\\r"); break;
            case '\t': buffer_append_str(buf, "\\t"); break;
            default:
                if (c < 0x20) {
                    buffer_append_format(buf, "\\u%04x", c);
                } else {
                    buffer_append_char(buf, (char)c);
                }
                break;
        }
    }
    buffer_append_char(buf, '"');
}

void send_response_with_headers(socket_t client, const char *status, const char *content_type,
                                const char *body, size_t body_len, const char *extra_headers) {
    const char *extra = extra_headers ? extra_headers : "";
    char header[768];
    int header_len = snprintf(header, sizeof header,
                              "HTTP/1.1 %s\r\n"
                              "Content-Type: %s\r\n"
                              "Content-Length: %zu\r\n"
                              "Connection: close\r\n"
                              "Access-Control-Allow-Origin: *\r\n"
                              "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                              "Access-Control-Allow-Headers: Content-Type\r\n"
                              "%s"
                              "\r\n",
                              status, content_type, body_len, extra);
    if (header_len > 0) {
        send(client, header, header_len, 0);
    }
    if (body_len > 0 && body) {
        send(client, body, (int)body_len, 0);
    }
}

void send_response(socket_t client, const char *status, const char *content_type,
                   const char *body, size_t body_len) {
    send_response_with_headers(client, status, content_type, body, body_len, NULL);
}

void send_empty_response(socket_t client, const char *status) {
    send_response(client, status, "text/plain; charset=utf-8", "", 0);
}

void send_json_response(socket_t client, const char *status, const char *json, size_t len) {
    send_response(client, status, "application/json; charset=utf-8", json, len);
}

void send_json_string(socket_t client, const char *status, const char *json) {
    send_json_response(client, status, json, strlen(json));
}

void send_error(socket_t client, const char *status, const char *message) {
    Buffer buf;
    if (buffer_init(&buf) != 0) {
        send_json_string(client, "500 Internal Server Error", "{\"error\":\"Interner Fehler\"}");
        return;
    }
    buffer_append_str(&buf, "{\"error\":");
    append_json_string(&buf, message ? message : "Unbekannter Fehler");
    buffer_append_char(&buf, '}');
    send_json_response(client, status, buf.data, buf.len);
    buffer_free(&buf);
}

void handle_options(socket_t client) {
    send_empty_response(client, "204 No Content");
}

static void handle_client(socket_t client) {
    char buffer[MAX_REQUEST_SIZE];
    size_t length = 0;
    if (read_http_request(client, buffer, sizeof buffer, &length) != 0) {
        send_error(client, "400 Bad Request", "Anfrage konnte nicht gelesen werden");
        return;
    }
    HttpRequest req;
    if (parse_http_request(buffer, length, &req) != 0) {
        send_error(client, "400 Bad Request", "Anfrage ist ungültig");
        return;
    }
    route_request(client, &req);
}

static int initialize_socket(socket_t *out_socket) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return -1;
    }
#endif
    socket_t server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) {
#ifdef _WIN32
        WSACleanup();
#endif
        return -1;
    }
    int opt = 1;
#ifdef _WIN32
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char *)&opt, sizeof opt);
#else
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof opt);
#endif
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof addr);
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((uint16_t) g_config.webserver_port);
    if (bind(server, (struct sockaddr *)&addr, sizeof addr) == SOCKET_ERROR) {
        close_socket(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return -1;
    }
    if (listen(server, 16) == SOCKET_ERROR) {
        close_socket(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return -1;
    }
    *out_socket = server;
    return 0;
}

int run_server(void) {
    socket_t server;
    if (initialize_socket(&server) != 0) {
        fprintf(stderr, "Server konnte nicht gestartet werden\n");
        return 1;
    }
    printf("Server läuft auf http://localhost:%d\n", g_config.webserver_port);
    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t addr_len = sizeof client_addr;
        socket_t client = accept(server, (struct sockaddr *)&client_addr, &addr_len);
        if (client == INVALID_SOCKET) {
            continue;
        }
        handle_client(client);
        close_socket(client);
    }
    close_socket(server);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
