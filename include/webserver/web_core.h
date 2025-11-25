#ifndef WEB_CORE_H
#define WEB_CORE_H

#include <stddef.h>
#include <stdarg.h>
#include <stdint.h>

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;
#define close_socket closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int socket_t;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define close_socket close
#endif

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buffer;

int buffer_init(Buffer *buf);
int buffer_append(Buffer *buf, const char *data, size_t data_len);
int buffer_append_str(Buffer *buf, const char *text);
int buffer_append_char(Buffer *buf, char c);
int buffer_append_format(Buffer *buf, const char *fmt, ...);
void buffer_free(Buffer *buf);

void append_json_string(Buffer *buf, const char *text);

void send_response_with_headers(socket_t client, const char *status, const char *content_type,
                                const char *body, size_t body_len, const char *extra_headers);
void send_response(socket_t client, const char *status, const char *content_type,
                   const char *body, size_t body_len);
void send_empty_response(socket_t client, const char *status);
void send_json_response(socket_t client, const char *status, const char *json, size_t len);
void send_json_string(socket_t client, const char *status, const char *json);
void send_error(socket_t client, const char *status, const char *message);
void handle_options(socket_t client);

int run_server(void);

#endif
