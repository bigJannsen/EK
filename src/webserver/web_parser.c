#include "webserver/web_parser.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int hex_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

static void url_decode(const char *src, char *dest, size_t dest_size) {
    size_t di = 0;
    for (size_t i = 0; src[i] && di + 1 < dest_size; ) {
        if (src[i] == '%' && src[i + 1] != '\0' && src[i + 2] != '\0') {
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

static void copy_and_decode_segment(const char *start, size_t length, char *decoded, size_t decoded_size) {
    if (decoded_size == 0) {
        return;
    }
    size_t raw_capacity = decoded_size * 3;
    if (raw_capacity == 0) {
        decoded[0] = '\0';
        return;
    }
    size_t usable = length;
    if (usable >= raw_capacity) {
        usable = raw_capacity - 1;
    }
    char raw[raw_capacity];
    if (usable > 0) {
        memcpy(raw, start, usable);
    }
    raw[usable] = '\0';
    url_decode(raw, decoded, decoded_size);
}

static void set_param(Param *param, const char *key, const char *value) {
    strncpy(param->key, key, MAX_KEY_LEN - 1);
    param->key[MAX_KEY_LEN - 1] = '\0';
    strncpy(param->value, value, MAX_VALUE_LEN - 1);
    param->value[MAX_VALUE_LEN - 1] = '\0';
}

static int parse_form_params(const char *data, Param *params, int max_params) {
    if (data == NULL) {
        return 0;
    }
    int count = 0;
    const char *p = data;
    while (*p && count < max_params) {
        const char *amp = strchr(p, '&');
        const char *end = amp ? amp : (p + strlen(p));
        const char *eq = memchr(p, '=', (size_t)(end - p));
        char key_buf[MAX_KEY_LEN];
        char value_buf[MAX_VALUE_LEN];
        if (eq != NULL) {
            copy_and_decode_segment(p, (size_t)(eq - p), key_buf, sizeof key_buf);
            copy_and_decode_segment(eq + 1, (size_t)(end - eq - 1), value_buf, sizeof value_buf);
        } else {
            copy_and_decode_segment(p, (size_t)(end - p), key_buf, sizeof key_buf);
            value_buf[0] = '\0';
        }
        set_param(&params[count], key_buf, value_buf);
        count++;
        if (amp == NULL) {
            break;
        }
        p = amp + 1;
    }
    return count;
}

const char *find_param(const Param *params, int count, const char *key) {
    for (int i = 0; i < count; i++) {
        if (strcmp(params[i].key, key) == 0) {
            return params[i].value;
        }
    }
    return NULL;
}

static int case_insensitive_prefix(const char *text, const char *prefix) {
    while (*prefix) {
        if (*text == '\0') {
            return 0;
        }
        if (tolower((unsigned char)*text) != tolower((unsigned char)*prefix)) {
            return 0;
        }
        text++;
        prefix++;
    }
    return 1;
}

int parse_http_request(char *buffer, size_t length, HttpRequest *req) {
    (void)length;
    memset(req, 0, sizeof *req);
    char *header_end = strstr(buffer, "\r\n\r\n");
    if (header_end == NULL) {
        return -1;
    }
    char *line_end = strstr(buffer, "\r\n");
    if (line_end == NULL) {
        return -1;
    }
    *line_end = '\0';
    char target[512];
    char version[32];
    if (sscanf(buffer, "%7s %511s %31s", req->method, target, version) != 3) {
        return -1;
    }
    char *query = strchr(target, '?');
    if (query != NULL) {
        *query = '\0';
        strncpy(req->query, query + 1, sizeof req->query - 1);
        req->query[sizeof req->query - 1] = '\0';
    }
    strncpy(req->path, target, sizeof req->path - 1);
    req->path[sizeof req->path - 1] = '\0';
    char *headers = line_end + 2;
    while (headers < header_end) {
        char *next = strstr(headers, "\r\n");
        if (next == NULL || headers == next) {
            break;
        }
        *next = '\0';
        char *colon = strchr(headers, ':');
        if (colon != NULL) {
            *colon = '\0';
            char *value = colon + 1;
            while (*value != '\0' && isspace((unsigned char)*value)) {
                value++;
            }
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

int read_http_request(socket_t client, char *buffer, size_t buffer_size, size_t *out_len) {
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
        if (headers_parsed == 0) {
            char *header_end = strstr(buffer, "\r\n\r\n");
            if (header_end != NULL) {
                headers_parsed = 1;
                header_len = (size_t)(header_end + 4 - buffer);
                char headers_copy[4096];
                size_t copy_len = header_len < sizeof headers_copy - 1 ? header_len : sizeof headers_copy - 1;
                memcpy(headers_copy, buffer, copy_len);
                headers_copy[copy_len] = '\0';
                char *line = strstr(headers_copy, "\r\n");
                if (line != NULL) {
                    line += 2;
                }
                while (line != NULL && *line != '\0') {
                    char *next = strstr(line, "\r\n");
                    if (next == NULL) {
                        break;
                    }
                    if (line == next) {
                        break;
                    }
                    *next = '\0';
                    char *colon = strchr(line, ':');
                    if (colon != NULL) {
                        *colon = '\0';
                        char *value = colon + 1;
                        while (*value != '\0' && isspace((unsigned char)*value)) {
                            value++;
                        }
                        if (case_insensitive_prefix(line, "Content-Length")) {
                            expected_body = (size_t)strtoul(value, NULL, 10);
                        }
                    }
                    line = next + 2;
                }
                if (expected_body == 0) {
                    break;
                }
                size_t have_body = total > header_len ? total - header_len : 0;
                if (have_body >= expected_body) {
                    break;
                }
            }
        } else {
            size_t have_body = total > header_len ? total - header_len : 0;
            if (have_body >= expected_body) {
                break;
            }
        }
    }
    if (headers_parsed == 0) {
        return -1;
    }
    if (out_len != NULL) {
        *out_len = total;
    }
    buffer[total] = '\0';
    return 0;
}
