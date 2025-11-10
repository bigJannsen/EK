#include "web_server.h"
#include "crud_database.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdarg.h>

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

#ifndef DATA_DIRECTORY
#define DATA_DIRECTORY "data"
#endif

#ifndef WEB_DIRECTORY
#define WEB_DIRECTORY "web"
#endif

#ifndef SHOPPING_LIST_PATH
#define SHOPPING_LIST_PATH "einkaufsliste.txt"
#endif

#define SERVER_PORT 8081
#define MAX_REQUEST_SIZE 65536
#define MAX_PARAMS 64
#define MAX_KEY_LEN 64
#define MAX_VALUE_LEN 512
#define MAX_DB_FILES 128
#define SHOPPING_LIST_MAX_ITEMS 100
#define SHOPPING_LIST_MAX_LEN 256

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

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} Buffer;

typedef enum {
    UNIT_UNKNOWN,
    UNIT_GRAM,
    UNIT_MILLILITER,
    UNIT_PIECE
} UnitType;

typedef struct {
    double amount;
    UnitType type;
} Quantity;

static int buffer_init(Buffer *buf) {
    buf->data = (char *)malloc(1024);
    if (!buf->data) return -1;
    buf->len = 0;
    buf->cap = 1024;
    buf->data[0] = '\0';
    return 0;
}

static int buffer_reserve(Buffer *buf, size_t needed) {
    if (buf->len + needed < buf->cap) return 0;
    size_t new_cap = buf->cap;
    while (new_cap < buf->len + needed + 1) {
        new_cap *= 2;
    }
    char *new_data = (char *)realloc(buf->data, new_cap);
    if (!new_data) return -1;
    buf->data = new_data;
    buf->cap = new_cap;
    return 0;
}

static int buffer_append(Buffer *buf, const char *data, size_t data_len) {
    if (buffer_reserve(buf, data_len) != 0) return -1;
    memcpy(buf->data + buf->len, data, data_len);
    buf->len += data_len;
    buf->data[buf->len] = '\0';
    return 0;
}

static int buffer_append_str(Buffer *buf, const char *text) {
    return buffer_append(buf, text, strlen(text));
}

static int buffer_append_char(Buffer *buf, char c) {
    return buffer_append(buf, &c, 1);
}

static int buffer_append_format(Buffer *buf, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    char temp[512];
    int written = vsnprintf(temp, sizeof temp, fmt, args);
    va_end(args);
    if (written < 0) return -1;
    if ((size_t)written >= sizeof temp) {
        size_t needed = (size_t)written + 1;
        char *dynamic = (char *)malloc(needed);
        if (!dynamic) return -1;
        va_start(args, fmt);
        vsnprintf(dynamic, needed, fmt, args);
        va_end(args);
        int res = buffer_append(buf, dynamic, (size_t)written);
        free(dynamic);
        return res;
    }
    return buffer_append(buf, temp, (size_t)written);
}

static void buffer_free(Buffer *buf) {
    free(buf->data);
    buf->data = NULL;
    buf->len = 0;
    buf->cap = 0;
}

static int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

static void url_decode(const char *src, char *dest, size_t dest_size) {
    size_t di = 0;
    for (size_t i = 0; src[i] && di + 1 < dest_size; ) {
        if (src[i] == '%' && src[i + 1] && src[i + 2]) {
            int h1 = hex_value(src[i + 1]);
            int h2 = hex_value(src[i + 2]);
            if (h1 >= 0 && h2 >= 0) {
                dest[di++] = (char)((h1 << 4) | h2);
                i += 3;
                continue;
            }
        }
        if (src[i] == '+') {
            dest[di++] = ' ';
            i++;
        } else {
            dest[di++] = src[i++];
        }
    }
    dest[di] = '\0';
}

static int parse_form_params(const char *data, Param *params, int max_params) {
    if (!data) return 0;
    int count = 0;
    const char *p = data;
    while (*p && count < max_params) {
        const char *amp = strchr(p, '&');
        const char *end = amp ? amp : (p + strlen(p));
        const char *eq = memchr(p, '=', (size_t)(end - p));
        char key_buf[MAX_KEY_LEN];
        char value_buf[MAX_VALUE_LEN];
        if (eq) {
            size_t key_len = (size_t)(eq - p);
            size_t val_len = (size_t)(end - eq - 1);
            char key_raw[MAX_KEY_LEN * 3];
            char val_raw[MAX_VALUE_LEN * 3];
            if (key_len >= sizeof key_raw) key_len = sizeof key_raw - 1;
            if (val_len >= sizeof val_raw) val_len = sizeof val_raw - 1;
            memcpy(key_raw, p, key_len);
            key_raw[key_len] = '\0';
            memcpy(val_raw, eq + 1, val_len);
            val_raw[val_len] = '\0';
            url_decode(key_raw, key_buf, sizeof key_buf);
            url_decode(val_raw, value_buf, sizeof value_buf);
        } else {
            char key_raw[MAX_KEY_LEN * 3];
            size_t key_len = (size_t)(end - p);
            if (key_len >= sizeof key_raw) key_len = sizeof key_raw - 1;
            memcpy(key_raw, p, key_len);
            key_raw[key_len] = '\0';
            url_decode(key_raw, key_buf, sizeof key_buf);
            value_buf[0] = '\0';
        }
        strncpy(params[count].key, key_buf, MAX_KEY_LEN - 1);
        params[count].key[MAX_KEY_LEN - 1] = '\0';
        strncpy(params[count].value, value_buf, MAX_VALUE_LEN - 1);
        params[count].value[MAX_VALUE_LEN - 1] = '\0';
        count++;
        if (!amp) break;
        p = amp + 1;
    }
    return count;
}

static const char *find_param(const Param *params, int count, const char *key) {
    for (int i = 0; i < count; i++) {
        if (strcmp(params[i].key, key) == 0) {
            return params[i].value;
        }
    }
    return NULL;
}

static int case_insensitive_prefix(const char *text, const char *prefix) {
    while (*prefix) {
        if (*text == '\0') return 0;
        if (tolower((unsigned char)*text) != tolower((unsigned char)*prefix)) return 0;
        text++;
        prefix++;
    }
    return 1;
}

static void append_json_string(Buffer *buf, const char *text) {
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

static const char *basename_of(const char *path) {
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *backslash = strrchr(path, '\\');
    if (!slash || (backslash && backslash > slash)) slash = backslash;
#endif
    return slash ? slash + 1 : path;
}

static int contains_path_traversal(const char *path) {
    if (!path) return 1;
    if (strstr(path, "..")) return 1;
    if (strstr(path, "\\")) return 1;
    return 0;
}

static int build_static_path(const char *relative, char *out, size_t out_size) {
    if (!relative || contains_path_traversal(relative)) return -1;
    while (*relative == '/') relative++;
    if (*relative == '\0') relative = "index.html";
    int written = snprintf(out, out_size, "%s/%s", WEB_DIRECTORY, relative);
    if (written < 0 || (size_t)written >= out_size) return -1;
    return 0;
}

static const char *content_type_for_path(const char *path) {
    const char *ext = strrchr(path, '.');
    if (!ext) return "application/octet-stream";
    if (strcmp(ext, ".html") == 0) return "text/html; charset=utf-8";
    if (strcmp(ext, ".css") == 0) return "text/css; charset=utf-8";
    if (strcmp(ext, ".js") == 0) return "application/javascript; charset=utf-8";
    if (strcmp(ext, ".json") == 0) return "application/json; charset=utf-8";
    if (strcmp(ext, ".png") == 0) return "image/png";
    if (strcmp(ext, ".jpg") == 0 || strcmp(ext, ".jpeg") == 0) return "image/jpeg";
    if (strcmp(ext, ".svg") == 0) return "image/svg+xml";
    return "application/octet-stream";
}

static void send_response_with_headers(socket_t client, const char *status, const char *content_type, const char *body, size_t body_len, const char *extra_headers) {
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

static void send_response(socket_t client, const char *status, const char *content_type, const char *body, size_t body_len) {
    send_response_with_headers(client, status, content_type, body, body_len, NULL);
}

static void send_empty_response(socket_t client, const char *status) {
    send_response(client, status, "text/plain; charset=utf-8", "", 0);
}

static void send_json_response(socket_t client, const char *status, const char *json, size_t len) {
    send_response(client, status, "application/json; charset=utf-8", json, len);
}

static void send_json_string(socket_t client, const char *status, const char *json) {
    send_json_response(client, status, json, strlen(json));
}

static void send_error(socket_t client, const char *status, const char *message) {
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

static int resolve_database_path(const char *name, char *out, size_t out_size) {
    if (!name || !*name) return -1;
    char files[MAX_DB_FILES][DB_MAX_FILENAME];
    int count = list_csv_files(DATA_DIRECTORY, files, MAX_DB_FILES);
    if (count <= 0) return -1;
    for (int i = 0; i < count; i++) {
        const char *base = basename_of(files[i]);
        if (strcmp(base, name) == 0) {
            strncpy(out, files[i], out_size - 1);
            out[out_size - 1] = '\0';
            return 0;
        }
    }
    return -1;
}

static int load_shopping_list(char items[SHOPPING_LIST_MAX_ITEMS][SHOPPING_LIST_MAX_LEN]) {
    FILE *file = fopen(SHOPPING_LIST_PATH, "r");
    if (!file) return 0;
    int count = 0;
    while (count < SHOPPING_LIST_MAX_ITEMS && fgets(items[count], SHOPPING_LIST_MAX_LEN, file)) {
        items[count][strcspn(items[count], "\r\n")] = '\0';
        count++;
    }
    fclose(file);
    return count;
}

static int save_shopping_list(char items[SHOPPING_LIST_MAX_ITEMS][SHOPPING_LIST_MAX_LEN], int count) {
    FILE *file = fopen(SHOPPING_LIST_PATH, "w");
    if (!file) return -1;
    for (int i = 0; i < count; i++) {
        fprintf(file, "%s\n", items[i]);
    }
    fclose(file);
    return 0;
}

static void split_list_entry(const char *line, char *article, size_t article_size, char *provider, size_t provider_size) {
    if (!line) {
        if (article && article_size) article[0] = '\0';
        if (provider && provider_size) provider[0] = '\0';
        return;
    }
    const char *sep = strchr(line, '|');
    if (sep) {
        size_t article_len = (size_t)(sep - line);
        if (article_size > 0) {
            if (article_len >= article_size) article_len = article_size - 1;
            memcpy(article, line, article_len);
            article[article_len] = '\0';
        }
        if (provider_size > 0) {
            strncpy(provider, sep + 1, provider_size - 1);
            provider[provider_size - 1] = '\0';
        }
    } else {
        if (article_size > 0) {
            strncpy(article, line, article_size - 1);
            article[article_size - 1] = '\0';
        }
        if (provider_size > 0) provider[0] = '\0';
    }
    if (article) {
        size_t len = strlen(article);
        while (len > 0 && isspace((unsigned char)article[len - 1])) article[--len] = '\0';
        size_t start = 0;
        while (article[start] && isspace((unsigned char)article[start])) start++;
        if (start > 0) memmove(article, article + start, strlen(article + start) + 1);
    }
    if (provider) {
        size_t len = strlen(provider);
        while (len > 0 && isspace((unsigned char)provider[len - 1])) provider[--len] = '\0';
        size_t start = 0;
        while (provider[start] && isspace((unsigned char)provider[start])) start++;
        if (start > 0) memmove(provider, provider + start, strlen(provider + start) + 1);
    }
}

static int unit_normalize(const char *unit, UnitType *type, double *factor) {
    if (!unit || !type || !factor) return -1;
    if (strcmp(unit, "g") == 0) {
        *type = UNIT_GRAM;
        *factor = 1.0;
        return 0;
    }
    if (strcmp(unit, "kg") == 0) {
        *type = UNIT_GRAM;
        *factor = 1000.0;
        return 0;
    }
    if (strcmp(unit, "l") == 0) {
        *type = UNIT_MILLILITER;
        *factor = 1000.0;
        return 0;
    }
    if (strcmp(unit, "ml") == 0) {
        *type = UNIT_MILLILITER;
        *factor = 1.0;
        return 0;
    }
    if (strcmp(unit, "stk") == 0 || strcmp(unit, "st") == 0 || strcmp(unit, "stück") == 0) {
        *type = UNIT_PIECE;
        *factor = 1.0;
        return 0;
    }
    return -1;
}

static int entry_to_quantity(const DatabaseEntry *entry, Quantity *quantity) {
    if (!entry || !quantity) return -1;
    if (entry->menge_wert <= 0.0) return -1;
    UnitType type = UNIT_UNKNOWN;
    double factor = 0.0;
    if (unit_normalize(entry->menge_einheit, &type, &factor) != 0) return -1;
    quantity->amount = entry->menge_wert * factor;
    quantity->type = type;
    return 0;
}

static const char *unit_label(UnitType type) {
    switch (type) {
        case UNIT_GRAM: return "g";
        case UNIT_MILLILITER: return "ml";
        case UNIT_PIECE: return "Stück";
        default: return "";
    }
}

static void append_menge_json(Buffer *buf, const DatabaseEntry *entry) {
    buffer_append_str(buf, ",\"mengeWert\":");
    char mengen_text[32];
    formatiere_mengenwert(entry->menge_wert, mengen_text, sizeof mengen_text);
    buffer_append_format(buf, "%s", mengen_text);
    buffer_append_str(buf, ",\"mengeEinheit\":");
    append_json_string(buf, entry->menge_einheit);
}

static int parse_http_request(char *buffer, size_t length, HttpRequest *req) {
    (void)length;
    memset(req, 0, sizeof *req);
    char *header_end = strstr(buffer, "\r\n\r\n");
    if (!header_end) return -1;
    char *line_end = strstr(buffer, "\r\n");
    if (!line_end) return -1;
    *line_end = '\0';
    char target[512];
    char version[32];
    if (sscanf(buffer, "%7s %511s %31s", req->method, target, version) != 3) {
        return -1;
    }
    char *query = strchr(target, '?');
    if (query) {
        *query = '\0';
        strncpy(req->query, query + 1, sizeof req->query - 1);
        req->query[sizeof req->query - 1] = '\0';
    }
    strncpy(req->path, target, sizeof req->path - 1);
    req->path[sizeof req->path - 1] = '\0';
    char *headers = line_end + 2;
    while (headers < header_end) {
        char *next = strstr(headers, "\r\n");
        if (!next || headers == next) break;
        *next = '\0';
        char *colon = strchr(headers, ':');
        if (colon) {
            *colon = '\0';
            char *value = colon + 1;
            while (*value && isspace((unsigned char)*value)) value++;
            if (case_insensitive_prefix(headers, "Content-Type")) {
                strncpy(req->content_type, value, sizeof req->content_type - 1);
                req->content_type[sizeof req->content_type - 1] = '\0';
            } else if (case_insensitive_prefix(headers, "Content-Length")) {
                req->content_length = (size_t)strtoul(value, NULL, 10);
            }
        }
        headers = next + 2;
    }
    req->body = header_end + 4;
    if (req->content_length > 0) {
        req->body[req->content_length] = '\0';
    }
    req->query_count = parse_form_params(req->query, req->query_params, MAX_PARAMS);
    if (req->content_length > 0 && case_insensitive_prefix(req->content_type, "application/x-www-form-urlencoded")) {
        req->body_count = parse_form_params(req->body, req->body_params, MAX_PARAMS);
    }
    return 0;
}

static int read_http_request(socket_t client, char *buffer, size_t buffer_size, size_t *out_len) {
    size_t total = 0;
    size_t header_len = 0;
    size_t expected_body = 0;
    int headers_parsed = 0;
    while (total + 1 < buffer_size) {
        int received = recv(client, buffer + total, (int)(buffer_size - total - 1), 0);
        if (received <= 0) {
            return -1;
        }
        total += (size_t)received;
        buffer[total] = '\0';
        if (!headers_parsed) {
            char *header_end = strstr(buffer, "\r\n\r\n");
            if (header_end) {
                headers_parsed = 1;
                header_len = (size_t)(header_end + 4 - buffer);
                char headers_copy[4096];
                size_t copy_len = header_len < sizeof headers_copy - 1 ? header_len : sizeof headers_copy - 1;
                memcpy(headers_copy, buffer, copy_len);
                headers_copy[copy_len] = '\0';
                char *line = strstr(headers_copy, "\r\n");
                if (line) line += 2;
                while (line && *line) {
                    char *next = strstr(line, "\r\n");
                    if (!next) break;
                    if (line == next) break;
                    *next = '\0';
                    char *colon = strchr(line, ':');
                    if (colon) {
                        *colon = '\0';
                        char *value = colon + 1;
                        while (*value && isspace((unsigned char)*value)) value++;
                        if (case_insensitive_prefix(line, "Content-Length")) {
                            expected_body = (size_t)strtoul(value, NULL, 10);
                        }
                    }
                    line = next + 2;
                }
                if (expected_body == 0) break;
                size_t have_body = total > header_len ? total - header_len : 0;
                if (have_body >= expected_body) break;
            }
        } else {
            size_t have_body = total > header_len ? total - header_len : 0;
            if (have_body >= expected_body) break;
        }
    }
    if (!headers_parsed) return -1;
    if (out_len) *out_len = total;
    buffer[total] = '\0';
    return 0;
}

static void send_file_response(socket_t client, const char *path) {
    FILE *file = fopen(path, "rb");
    if (!file) {
        send_error(client, "404 Not Found", "Datei nicht gefunden");
        return;
    }
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        send_error(client, "500 Internal Server Error", "Dateizugriff fehlgeschlagen");
        return;
    }
    long size = ftell(file);
    if (size < 0) {
        fclose(file);
        send_error(client, "500 Internal Server Error", "Dateigröße unbekannt");
        return;
    }
    rewind(file);
    char *data = (char *)malloc((size_t)size);
    if (!data) {
        fclose(file);
        send_error(client, "500 Internal Server Error", "Speicherfehler");
        return;
    }
    size_t read_bytes = fread(data, 1, (size_t)size, file);
    fclose(file);
    if (read_bytes != (size_t)size) {
        free(data);
        send_error(client, "500 Internal Server Error", "Dateilesefehler");
        return;
    }
    const char *content_type = content_type_for_path(path);
    send_response(client, "200 OK", content_type, data, (size_t)size);
    free(data);
}

static void handle_get_root(socket_t client) {
    char path[512];
    if (build_static_path("index.html", path, sizeof path) != 0) {
        send_error(client, "500 Internal Server Error", "Frontend nicht gefunden");
        return;
    }
    send_file_response(client, path);
}

static void handle_get_static(socket_t client, const char *relative) {
    char path[512];
    if (build_static_path(relative, path, sizeof path) != 0) {
        send_error(client, "400 Bad Request", "Ungültiger Pfad");
        return;
    }
    send_file_response(client, path);
}

static void handle_db_files(socket_t client) {
    char files[MAX_DB_FILES][DB_MAX_FILENAME];
    int count = list_csv_files(DATA_DIRECTORY, files, MAX_DB_FILES);
    Buffer buf;
    if (buffer_init(&buf) != 0) {
        send_error(client, "500 Internal Server Error", "Speicherfehler");
        return;
    }
    buffer_append_str(&buf, "{\"files\":[");
    for (int i = 0; i < count; i++) {
        if (i > 0) buffer_append_char(&buf, ',');
        buffer_append_str(&buf, "{\"name\":");
        append_json_string(&buf, basename_of(files[i]));
        buffer_append_char(&buf, '}');
    }
    buffer_append_str(&buf, "]}");
    send_json_response(client, "200 OK", buf.data, buf.len);
    buffer_free(&buf);
}

static void handle_db_get(socket_t client, const HttpRequest *req) {
    const char *name = find_param(req->query_params, req->query_count, "name");
    if (!name || !*name) {
        send_error(client, "400 Bad Request", "Parameter 'name' fehlt");
        return;
    }
    char path[DB_MAX_FILENAME];
    if (resolve_database_path(name, path, sizeof path) != 0) {
        send_error(client, "404 Not Found", "Datenbank nicht gefunden");
        return;
    }
    Database db;
    if (load_database(path, &db) != 0) {
        send_error(client, "500 Internal Server Error", "Datenbank konnte nicht geladen werden");
        return;
    }
    Buffer buf;
    if (buffer_init(&buf) != 0) {
        send_error(client, "500 Internal Server Error", "Speicherfehler");
        return;
    }
    buffer_append_str(&buf, "{\"name\":");
    append_json_string(&buf, name);
    buffer_append_str(&buf, ",\"entries\":[");
    for (int i = 0; i < db.anzahl; i++) {
        if (i > 0) buffer_append_char(&buf, ',');
        DatabaseEntry *entry = &db.eintraege[i];
        buffer_append_str(&buf, "{\"id\":");
        buffer_append_format(&buf, "%d", entry->id);
        buffer_append_str(&buf, ",\"artikel\":");
        append_json_string(&buf, entry->artikel);
        buffer_append_str(&buf, ",\"anbieter\":");
        append_json_string(&buf, entry->anbieter);
        buffer_append_str(&buf, ",\"preisCent\":");
        buffer_append_format(&buf, "%d", entry->preis_ct);
        buffer_append_str(&buf, ",\"preisEuro\":");
        buffer_append_format(&buf, "%.2f", entry->preis_ct / 100.0);
        append_menge_json(&buf, entry);
        buffer_append_char(&buf, '}');
    }
    buffer_append_str(&buf, "]}");
    send_json_response(client, "200 OK", buf.data, buf.len);
    buffer_free(&buf);
}

static int parse_int_param(const char *value, int *out) {
    if (!value) return -1;
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (!end || *end != '\0') return -1;
    *out = (int)v;
    return 0;
}

static int parse_double_param(const char *value, double *out) {
    if (!value) return -1;
    char buffer[64];
    strncpy(buffer, value, sizeof buffer - 1);
    buffer[sizeof buffer - 1] = '\0';
    for (char *c = buffer; *c; ++c) {
        if (*c == ',') *c = '.';
    }
    char *end = NULL;
    double v = strtod(buffer, &end);
    if (!end || *end != '\0') return -1;
    *out = v;
    return 0;
}

static void handle_db_add_or_update(socket_t client, const HttpRequest *req, int is_update) {
    const char *name = find_param(req->body_params, req->body_count, "name");
    const char *id_text = find_param(req->body_params, req->body_count, "id");
    const char *artikel = find_param(req->body_params, req->body_count, "artikel");
    const char *anbieter = find_param(req->body_params, req->body_count, "anbieter");
    const char *preis_text = find_param(req->body_params, req->body_count, "preisCent");
    const char *menge_wert_text = find_param(req->body_params, req->body_count, "mengeWert");
    const char *menge_einheit_text = find_param(req->body_params, req->body_count, "mengeEinheit");
    if (!name || !id_text || !artikel || !anbieter || !preis_text || !menge_wert_text || !menge_einheit_text) {
        send_error(client, "400 Bad Request", "Parameter fehlen");
        return;
    }
    int id = 0;
    if (parse_int_param(id_text, &id) != 0) {
        send_error(client, "400 Bad Request", "Ungültige ID");
        return;
    }
    int preis = 0;
    if (parse_int_param(preis_text, &preis) != 0) {
        send_error(client, "400 Bad Request", "Ungültiger Preis");
        return;
    }
    double mengenwert_roh = 0.0;
    if (parse_double_param(menge_wert_text, &mengenwert_roh) != 0 || mengenwert_roh <= 0.0) {
        send_error(client, "400 Bad Request", "Ungültiger Mengenwert");
        return;
    }
    double mengenwert = mengenwert_roh;
    char menge_einheit_norm[sizeof(((DatabaseEntry *)0)->menge_einheit)];
    if (normalisiere_mengeneinheit(menge_einheit_text, menge_einheit_norm, sizeof menge_einheit_norm, &mengenwert) != 0) {
        send_error(client, "400 Bad Request", "Ungültige Einheit");
        return;
    }
    char path[DB_MAX_FILENAME];
    if (resolve_database_path(name, path, sizeof path) != 0) {
        send_error(client, "404 Not Found", "Datenbank nicht gefunden");
        return;
    }
    Database db;
    if (load_database(path, &db) != 0) {
        send_error(client, "500 Internal Server Error", "Datenbank konnte nicht geladen werden");
        return;
    }
    DatabaseEntry *entry = NULL;
    for (int i = 0; i < db.anzahl; i++) {
        if (db.eintraege[i].id == id) {
            entry = &db.eintraege[i];
            break;
        }
    }
    if (is_update) {
        if (!entry) {
            send_error(client, "404 Not Found", "Eintrag nicht gefunden");
            return;
        }
    } else {
        if (entry) {
            send_error(client, "400 Bad Request", "ID existiert bereits");
            return;
        }
        if (db.anzahl >= DB_MAX_ENTRIES) {
            send_error(client, "400 Bad Request", "Keine freien Plätze");
            return;
        }
        entry = &db.eintraege[db.anzahl++];
        entry->id = id;
    }
    strncpy(entry->artikel, artikel, DB_MAX_TEXT - 1);
    entry->artikel[DB_MAX_TEXT - 1] = '\0';
    strncpy(entry->anbieter, anbieter, DB_MAX_TEXT - 1);
    entry->anbieter[DB_MAX_TEXT - 1] = '\0';
    entry->preis_ct = preis;
    entry->menge_wert = mengenwert;
    strncpy(entry->menge_einheit, menge_einheit_norm, sizeof(entry->menge_einheit) - 1);
    entry->menge_einheit[sizeof(entry->menge_einheit) - 1] = '\0';
    if (save_database(&db) != 0) {
        send_error(client, "500 Internal Server Error", "Speichern fehlgeschlagen");
        return;
    }
    Buffer buf;
    if (buffer_init(&buf) != 0) {
        send_error(client, "500 Internal Server Error", "Speicherfehler");
        return;
    }
    buffer_append_str(&buf, "{\"status\":\"ok\",\"entry\":{");
    buffer_append_str(&buf, "\"id\":");
    buffer_append_format(&buf, "%d", entry->id);
    buffer_append_str(&buf, ",\"artikel\":");
    append_json_string(&buf, entry->artikel);
    buffer_append_str(&buf, ",\"anbieter\":");
    append_json_string(&buf, entry->anbieter);
    buffer_append_str(&buf, ",\"preisCent\":");
    buffer_append_format(&buf, "%d", entry->preis_ct);
    append_menge_json(&buf, entry);
    buffer_append_str(&buf, "}}");
    send_json_response(client, "200 OK", buf.data, buf.len);
    buffer_free(&buf);
}

static void handle_db_delete(socket_t client, const HttpRequest *req) {
    const char *name = find_param(req->body_params, req->body_count, "name");
    const char *id_text = find_param(req->body_params, req->body_count, "id");
    if (!name || !id_text) {
        send_error(client, "400 Bad Request", "Parameter fehlen");
        return;
    }
    int id = 0;
    if (parse_int_param(id_text, &id) != 0) {
        send_error(client, "400 Bad Request", "Ungültige ID");
        return;
    }
    char path[DB_MAX_FILENAME];
    if (resolve_database_path(name, path, sizeof path) != 0) {
        send_error(client, "404 Not Found", "Datenbank nicht gefunden");
        return;
    }
    Database db;
    if (load_database(path, &db) != 0) {
        send_error(client, "500 Internal Server Error", "Datenbank konnte nicht geladen werden");
        return;
    }
    int index = -1;
    for (int i = 0; i < db.anzahl; i++) {
        if (db.eintraege[i].id == id) {
            index = i;
            break;
        }
    }
    if (index < 0) {
        send_error(client, "404 Not Found", "Eintrag nicht gefunden");
        return;
    }
    for (int i = index + 1; i < db.anzahl; i++) {
        db.eintraege[i - 1] = db.eintraege[i];
    }
    db.anzahl--;
    if (save_database(&db) != 0) {
        send_error(client, "500 Internal Server Error", "Speichern fehlgeschlagen");
        return;
    }
    send_json_string(client, "200 OK", "{\"status\":\"ok\"}");
}

static void handle_list_get(socket_t client) {
    char items[SHOPPING_LIST_MAX_ITEMS][SHOPPING_LIST_MAX_LEN];
    int count = load_shopping_list(items);
    Buffer buf;
    if (buffer_init(&buf) != 0) {
        send_error(client, "500 Internal Server Error", "Speicherfehler");
        return;
    }
    buffer_append_str(&buf, "{\"items\":[");
    for (int i = 0; i < count; i++) {
        if (i > 0) buffer_append_char(&buf, ',');
        char article[DB_MAX_TEXT];
        char provider[DB_MAX_TEXT];
        split_list_entry(items[i], article, sizeof article, provider, sizeof provider);
        buffer_append_str(&buf, "{\"index\":");
        buffer_append_format(&buf, "%d", i);
        buffer_append_str(&buf, ",\"artikel\":");
        append_json_string(&buf, article);
        buffer_append_str(&buf, ",\"anbieter\":");
        append_json_string(&buf, provider);
        buffer_append_str(&buf, ",\"text\":");
        append_json_string(&buf, items[i]);
        buffer_append_char(&buf, '}');
    }
    buffer_append_str(&buf, "]}");
    send_json_response(client, "200 OK", buf.data, buf.len);
    buffer_free(&buf);
}

static void handle_list_download(socket_t client) {
    char items[SHOPPING_LIST_MAX_ITEMS][SHOPPING_LIST_MAX_LEN];
    int count = load_shopping_list(items);
    Buffer buf;
    if (buffer_init(&buf) != 0) {
        send_error(client, "500 Internal Server Error", "Speicherfehler");
        return;
    }
    for (int i = 0; i < count; i++) {
        if (buffer_append_str(&buf, items[i]) != 0 || buffer_append_char(&buf, '\n') != 0) {
            buffer_free(&buf);
            send_error(client, "500 Internal Server Error", "Speicherfehler");
            return;
        }
    }
    const char *disposition = "Content-Disposition: attachment; filename=\"einkaufsliste.txt\"\r\n";
    send_response_with_headers(client, "200 OK", "text/plain; charset=utf-8", buf.data, buf.len, disposition);
    buffer_free(&buf);
}

static void build_list_entry(const char *artikel, const char *anbieter, char *out, size_t out_size) {
    if (anbieter && *anbieter) {
        snprintf(out, out_size, "%s|%s", artikel, anbieter);
    } else {
        snprintf(out, out_size, "%s", artikel);
    }
    out[out_size - 1] = '\0';
}

static void handle_list_add_or_update(socket_t client, const HttpRequest *req, int is_update) {
    const char *artikel = find_param(req->body_params, req->body_count, "artikel");
    const char *anbieter = find_param(req->body_params, req->body_count, "anbieter");
    const char *index_text = find_param(req->body_params, req->body_count, "index");
    if (!artikel || !*artikel) {
        send_error(client, "400 Bad Request", "Artikel fehlt");
        return;
    }
    char items[SHOPPING_LIST_MAX_ITEMS][SHOPPING_LIST_MAX_LEN];
    int count = load_shopping_list(items);
    if (is_update) {
        if (!index_text) {
            send_error(client, "400 Bad Request", "Index fehlt");
            return;
        }
        int index = 0;
        if (parse_int_param(index_text, &index) != 0 || index < 0 || index >= count) {
            send_error(client, "400 Bad Request", "Ungültiger Index");
            return;
        }
        build_list_entry(artikel, anbieter ? anbieter : "", items[index], sizeof items[index]);
    } else {
        if (count >= SHOPPING_LIST_MAX_ITEMS) {
            send_error(client, "400 Bad Request", "Liste voll");
            return;
        }
        build_list_entry(artikel, anbieter ? anbieter : "", items[count], sizeof items[count]);
        count++;
    }
    if (save_shopping_list(items, count) != 0) {
        send_error(client, "500 Internal Server Error", "Speichern fehlgeschlagen");
        return;
    }
    send_json_string(client, "200 OK", "{\"status\":\"ok\"}");
}

static void handle_list_delete(socket_t client, const HttpRequest *req) {
    const char *index_text = find_param(req->body_params, req->body_count, "index");
    if (!index_text) {
        send_error(client, "400 Bad Request", "Index fehlt");
        return;
    }
    int index = 0;
    if (parse_int_param(index_text, &index) != 0) {
        send_error(client, "400 Bad Request", "Ungültiger Index");
        return;
    }
    char items[SHOPPING_LIST_MAX_ITEMS][SHOPPING_LIST_MAX_LEN];
    int count = load_shopping_list(items);
    if (index < 0 || index >= count) {
        send_error(client, "404 Not Found", "Eintrag nicht gefunden");
        return;
    }
    for (int i = index + 1; i < count; i++) {
        strncpy(items[i - 1], items[i], SHOPPING_LIST_MAX_LEN - 1);
        items[i - 1][SHOPPING_LIST_MAX_LEN - 1] = '\0';
    }
    count--;
    if (save_shopping_list(items, count) != 0) {
        send_error(client, "500 Internal Server Error", "Speichern fehlgeschlagen");
        return;
    }
    send_json_string(client, "200 OK", "{\"status\":\"ok\"}");
}

static void handle_compare_single(socket_t client, const HttpRequest *req) {
    const char *name = find_param(req->body_params, req->body_count, "name");
    const char *first_id_text = find_param(req->body_params, req->body_count, "firstId");
    const char *second_id_text = find_param(req->body_params, req->body_count, "secondId");
    const char *amount_text = find_param(req->body_params, req->body_count, "amount");
    if (!name || !first_id_text || !second_id_text || !amount_text) {
        send_error(client, "400 Bad Request", "Parameter fehlen");
        return;
    }
    int first_id = 0;
    int second_id = 0;
    double amount = 0.0;
    if (parse_int_param(first_id_text, &first_id) != 0 || parse_int_param(second_id_text, &second_id) != 0) {
        send_error(client, "400 Bad Request", "Ungültige IDs");
        return;
    }
    if (parse_double_param(amount_text, &amount) != 0 || amount <= 0.0) {
        send_error(client, "400 Bad Request", "Ungültige Menge");
        return;
    }
    char path[DB_MAX_FILENAME];
    if (resolve_database_path(name, path, sizeof path) != 0) {
        send_error(client, "404 Not Found", "Datenbank nicht gefunden");
        return;
    }
    Database db;
    if (load_database(path, &db) != 0) {
        send_error(client, "500 Internal Server Error", "Datenbank konnte nicht geladen werden");
        return;
    }
    DatabaseEntry *first = NULL;
    DatabaseEntry *second = NULL;
    for (int i = 0; i < db.anzahl; i++) {
        if (db.eintraege[i].id == first_id) first = &db.eintraege[i];
        if (db.eintraege[i].id == second_id) second = &db.eintraege[i];
    }
    if (!first || !second) {
        send_error(client, "404 Not Found", "Eintrag nicht gefunden");
        return;
    }
    Quantity qty_first;
    Quantity qty_second;
    if (entry_to_quantity(first, &qty_first) != 0 || entry_to_quantity(second, &qty_second) != 0) {
        send_error(client, "400 Bad Request", "Mengenangaben können nicht interpretiert werden");
        return;
    }
    if (qty_first.type != qty_second.type) {
        send_error(client, "400 Bad Request", "Einheiten sind nicht kompatibel");
        return;
    }
    double unit_price_first = (double)first->preis_ct / qty_first.amount;
    double unit_price_second = (double)second->preis_ct / qty_second.amount;
    double total_first = unit_price_first * amount;
    double total_second = unit_price_second * amount;
    const char *unit = unit_label(qty_first.type);
    const double epsilon = 1e-6;
    const char *cheaper = "equal";
    if (unit_price_first + epsilon < unit_price_second) cheaper = "first";
    else if (unit_price_second + epsilon < unit_price_first) cheaper = "second";
    Buffer buf;
    if (buffer_init(&buf) != 0) {
        send_error(client, "500 Internal Server Error", "Speicherfehler");
        return;
    }
    buffer_append_str(&buf, "{\"status\":\"ok\",\"unit\":");
    append_json_string(&buf, unit);
    buffer_append_str(&buf, ",\"amount\":");
    buffer_append_format(&buf, "%.4f", amount);
    buffer_append_str(&buf, ",\"cheaper\":");
    append_json_string(&buf, cheaper);
    buffer_append_str(&buf, ",\"first\":{");
    buffer_append_str(&buf, "\"id\":");
    buffer_append_format(&buf, "%d", first->id);
    buffer_append_str(&buf, ",\"artikel\":");
    append_json_string(&buf, first->artikel);
    buffer_append_str(&buf, ",\"anbieter\":");
    append_json_string(&buf, first->anbieter);
    append_menge_json(&buf, first);
    buffer_append_str(&buf, ",\"preisCent\":");
    buffer_append_format(&buf, "%d", first->preis_ct);
    buffer_append_str(&buf, ",\"unitPrice\":");
    buffer_append_format(&buf, "%.6f", unit_price_first);
    buffer_append_str(&buf, ",\"total\":");
    buffer_append_format(&buf, "%.6f", total_first);
    buffer_append_str(&buf, "},\"second\":{");
    buffer_append_str(&buf, "\"id\":");
    buffer_append_format(&buf, "%d", second->id);
    buffer_append_str(&buf, ",\"artikel\":");
    append_json_string(&buf, second->artikel);
    buffer_append_str(&buf, ",\"anbieter\":");
    append_json_string(&buf, second->anbieter);
    append_menge_json(&buf, second);
    buffer_append_str(&buf, ",\"preisCent\":");
    buffer_append_format(&buf, "%d", second->preis_ct);
    buffer_append_str(&buf, ",\"unitPrice\":");
    buffer_append_format(&buf, "%.6f", unit_price_second);
    buffer_append_str(&buf, ",\"total\":");
    buffer_append_format(&buf, "%.6f", total_second);
    buffer_append_str(&buf, "}}");
    send_json_response(client, "200 OK", buf.data, buf.len);
    buffer_free(&buf);
}

static void handle_compare_list(socket_t client, const HttpRequest *req) {
    const char *name = find_param(req->body_params, req->body_count, "name");
    const char *apply_text = find_param(req->body_params, req->body_count, "apply");
    int apply = (apply_text && strcmp(apply_text, "1") == 0) ? 1 : 0;
    if (!name || !*name) {
        send_error(client, "400 Bad Request", "Parameter 'name' fehlt");
        return;
    }
    char path[DB_MAX_FILENAME];
    if (resolve_database_path(name, path, sizeof path) != 0) {
        send_error(client, "404 Not Found", "Datenbank nicht gefunden");
        return;
    }
    Database db;
    if (load_database(path, &db) != 0) {
        send_error(client, "500 Internal Server Error", "Datenbank konnte nicht geladen werden");
        return;
    }
    char items[SHOPPING_LIST_MAX_ITEMS][SHOPPING_LIST_MAX_LEN];
    int count = load_shopping_list(items);
    Buffer buf;
    if (buffer_init(&buf) != 0) {
        send_error(client, "500 Internal Server Error", "Speicherfehler");
        return;
    }
    buffer_append_str(&buf, "{\"status\":\"ok\",\"updated\":");
    buffer_append_str(&buf, apply ? "true" : "false");
    buffer_append_str(&buf, ",\"items\":[");
    int changed = 0;
    for (int i = 0; i < count; i++) {
        if (i > 0) buffer_append_char(&buf, ',');
        char article[DB_MAX_TEXT];
        char provider[DB_MAX_TEXT];
        split_list_entry(items[i], article, sizeof article, provider, sizeof provider);
        buffer_append_str(&buf, "{\"index\":");
        buffer_append_format(&buf, "%d", i);
        buffer_append_str(&buf, ",\"artikel\":");
        append_json_string(&buf, article);
        buffer_append_str(&buf, ",\"aktuellerAnbieter\":");
        append_json_string(&buf, provider);
        int matches = 0;
        int best_idx = -1;
        int best_has_qty = 0;
        double best_unit_price = 0.0;
        int best_pack_price = 0;
        for (int j = 0; j < db.anzahl; j++) {
            DatabaseEntry *entry = &db.eintraege[j];
            if (strcmp(entry->artikel, article) != 0) continue;
            matches++;
            Quantity qty;
            int has_qty = 0;
            double unit_price = 0.0;
            if (entry_to_quantity(entry, &qty) == 0 && qty.amount > 0.0) {
                has_qty = 1;
                unit_price = (double)entry->preis_ct / qty.amount;
            }
            if (has_qty) {
                if (!best_has_qty || unit_price + 1e-6 < best_unit_price) {
                    best_idx = j;
                    best_has_qty = 1;
                    best_unit_price = unit_price;
                    best_pack_price = entry->preis_ct;
                }
            } else {
                if (best_idx == -1 || (!best_has_qty && entry->preis_ct < best_pack_price)) {
                    best_idx = j;
                    best_pack_price = entry->preis_ct;
                }
            }
        }
        buffer_append_str(&buf, ",\"anbieterGefunden\":");
        buffer_append_format(&buf, "%d", matches);
        if (best_idx >= 0) {
            DatabaseEntry *best = &db.eintraege[best_idx];
            buffer_append_str(&buf, ",\"empfehlung\":{");
            buffer_append_str(&buf, "\"anbieter\":");
            append_json_string(&buf, best->anbieter);
            append_menge_json(&buf, best);
            buffer_append_str(&buf, ",\"preisCent\":");
            buffer_append_format(&buf, "%d", best->preis_ct);
            Quantity best_qty;
            if (entry_to_quantity(best, &best_qty) == 0 && best_qty.amount > 0.0) {
                buffer_append_str(&buf, ",\"unitPrice\":");
                buffer_append_format(&buf, "%.6f", (double)best->preis_ct / best_qty.amount);
                buffer_append_str(&buf, ",\"unit\":");
                append_json_string(&buf, unit_label(best_qty.type));
            }
            buffer_append_char(&buf, '}');
            if (apply && strcmp(provider, best->anbieter) != 0) {
                changed = 1;
                build_list_entry(best->artikel, best->anbieter, items[i], sizeof items[i]);
            }
        }
        buffer_append_char(&buf, '}');
    }
    buffer_append_str(&buf, "]}");
    if (apply && changed) {
        save_shopping_list(items, count);
    }
    send_json_response(client, "200 OK", buf.data, buf.len);
    buffer_free(&buf);
}

static void handle_options(socket_t client) {
    send_empty_response(client, "204 No Content");
}

static void route_request(socket_t client, HttpRequest *req) {
    if (strcmp(req->method, "OPTIONS") == 0) {
        handle_options(client);
        return;
    }
    if (strcmp(req->method, "GET") == 0) {
        if (strcmp(req->path, "/") == 0) {
            handle_get_root(client);
            return;
        }
        if (strncmp(req->path, "/static/", 8) == 0) {
            handle_get_static(client, req->path + 8);
            return;
        }
        if (strcmp(req->path, "/api/db-files") == 0) {
            handle_db_files(client);
            return;
        }
        if (strcmp(req->path, "/api/db") == 0) {
            handle_db_get(client, req);
            return;
        }
        if (strcmp(req->path, "/api/list/download") == 0) {
            handle_list_download(client);
            return;
        }
        if (strcmp(req->path, "/api/list") == 0) {
            handle_list_get(client);
            return;
        }
        send_error(client, "404 Not Found", "Pfad nicht gefunden");
        return;
    }
    if (strcmp(req->method, "POST") == 0) {
        if (strcmp(req->path, "/api/db/add") == 0) {
            handle_db_add_or_update(client, req, 0);
            return;
        }
        if (strcmp(req->path, "/api/db/update") == 0) {
            handle_db_add_or_update(client, req, 1);
            return;
        }
        if (strcmp(req->path, "/api/db/delete") == 0) {
            handle_db_delete(client, req);
            return;
        }
        if (strcmp(req->path, "/api/list/add") == 0) {
            handle_list_add_or_update(client, req, 0);
            return;
        }
        if (strcmp(req->path, "/api/list/update") == 0) {
            handle_list_add_or_update(client, req, 1);
            return;
        }
        if (strcmp(req->path, "/api/list/delete") == 0) {
            handle_list_delete(client, req);
            return;
        }
        if (strcmp(req->path, "/api/compare/single") == 0) {
            handle_compare_single(client, req);
            return;
        }
        if (strcmp(req->path, "/api/compare/list") == 0) {
            handle_compare_list(client, req);
            return;
        }
        send_error(client, "404 Not Found", "Pfad nicht gefunden");
        return;
    }
    send_error(client, "405 Method Not Allowed", "Methode nicht erlaubt");
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
    addr.sin_port = htons(SERVER_PORT);
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
    printf("Server läuft auf http://localhost:%d\n", SERVER_PORT);
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

