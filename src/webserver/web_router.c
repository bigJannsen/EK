#include "webserver/web_router.h"

#include <string.h>

#include "webserver/web_api.h"
#include "webserver/web_core.h"
#include "webserver/web_static.h"

void route_request(socket_t client, const HttpRequest *req) {
    if (strcmp(req->method, "OPTIONS") == 0) {
        handle_options(client);
        return;
    }
    if (strcmp(req->method, "GET") == 0) {
        if (strcmp(req->path, "/") == 0) {
            handle_static_root(client);
            return;
        }
        if (strncmp(req->path, "/static/", 8) == 0) {
            handle_static_request(client, req->path + 8);
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
            handle_db_add(client, req);
            return;
        }
        if (strcmp(req->path, "/api/db/update") == 0) {
            handle_db_update(client, req);
            return;
        }
        if (strcmp(req->path, "/api/db/delete") == 0) {
            handle_db_delete(client, req);
            return;
        }
        if (strcmp(req->path, "/api/list/add") == 0) {
            handle_list_add(client, req);
            return;
        }
        if (strcmp(req->path, "/api/list/update") == 0) {
            handle_list_update(client, req);
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
