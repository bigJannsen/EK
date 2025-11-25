#include "webserver/web_api.h"

#include "webserver/api/api_compare_handlers.h"
#include "webserver/api/api_db_handlers.h"
#include "webserver/api/api_list_handlers.h"

void handle_db_files(socket_t client) {
    api_db_handle_files(client);
}

void handle_db_get(socket_t client, const HttpRequest *req) {
    api_db_handle_get(client, req);
}

void handle_db_add(socket_t client, const HttpRequest *req) {
    api_db_handle_add(client, req);
}

void handle_db_update(socket_t client, const HttpRequest *req) {
    api_db_handle_update(client, req);
}

void handle_db_delete(socket_t client, const HttpRequest *req) {
    api_db_handle_delete(client, req);
}

void handle_list_get(socket_t client) {
    api_list_handle_get(client);
}

void handle_list_add(socket_t client, const HttpRequest *req) {
    api_list_handle_add(client, req);
}

void handle_list_update(socket_t client, const HttpRequest *req) {
    api_list_handle_update(client, req);
}

void handle_list_delete(socket_t client, const HttpRequest *req) {
    api_list_handle_delete(client, req);
}

void handle_list_download(socket_t client) {
    api_list_handle_download(client);
}

void handle_compare_single(socket_t client, const HttpRequest *req) {
    api_compare_handle_single(client, req);
}

void handle_compare_list(socket_t client, const HttpRequest *req) {
    api_compare_handle_list(client, req);
}
