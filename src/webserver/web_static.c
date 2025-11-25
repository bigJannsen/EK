#include "webserver/web_static.h"

#include "webserver/web_core.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef WEB_DIRECTORY
#define WEB_DIRECTORY "web"
#endif

static int contains_path_traversal(const char *path) {
    if (path == NULL) {
        return 1;
    }
    if (strstr(path, "..") != NULL) {
        return 1;
    }
    if (strstr(path, "\\") != NULL) {
        return 1;
    }
    return 0;
}

int build_static_path(const char *relative, char *out, size_t out_size) {
    if (relative == NULL || contains_path_traversal(relative)) {
        return -1;
    }
    while (*relative == '/') {
        relative++;
    }
    if (*relative == '\0') {
        relative = "index.html";
    }
    int written = snprintf(out, out_size, "%s/%s", WEB_DIRECTORY, relative);
    if (written < 0 || (size_t)written >= out_size) {
        return -1;
    }
    return 0;
}

static const char *content_type_for_path(const char *path) {
    struct {
        const char *ext;
        const char *type;
    } types[] = {
        {".html", "text/html; charset=utf-8"},
        {".css",  "text/css; charset=utf-8"},
        {".js",   "application/javascript; charset=utf-8"},
        {".json", "application/json; charset=utf-8"},
        {".png",  "image/png"},
        {".jpg",  "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".svg",  "image/svg+xml"},
    };

    const char *ext = strrchr(path, '.');
    if (ext == NULL) {
        return "application/octet-stream";
    }

    for (size_t i = 0; i < sizeof(types) / sizeof(types[0]); i++) {
        if (strcmp(ext, types[i].ext) == 0) {
            return types[i].type;
        }
    }

    return "application/octet-stream";
}

void send_file_response(socket_t client, const char *path) {
    FILE *file = fopen(path, "rb");
    if (file == NULL) {
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
    if (data == NULL) {
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

void handle_static_root(socket_t client) {
    char path[512];
    if (build_static_path("index.html", path, sizeof path) != 0) {
        send_error(client, "500 Internal Server Error", "Frontend nicht gefunden");
        return;
    }
    send_file_response(client, path);
}

void handle_static_request(socket_t client, const char *relative) {
    char path[512];
    if (build_static_path(relative, path, sizeof path) != 0) {
        send_error(client, "400 Bad Request", "Ungültiger Pfad");
        return;
    }
    send_file_response(client, path);
}
