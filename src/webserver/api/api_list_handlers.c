#include "webserver/api/api_list_handlers.h"

#include "database/text_input_utils.h"

#include <string.h>

void api_list_handle_get(socket_t client) {
    char items[SHOPPING_LIST_MAX_ITEMS][SHOPPING_LIST_MAX_LEN];
    int count = load_shopping_list(items, SHOPPING_LIST_MAX_ITEMS);
    Buffer buf;
    if (buffer_init(&buf) != 0) {
        send_error(client, "500 Internal Server Error", "Speicherfehler");
        return;
    }
    buffer_append_str(&buf, "{\"items\":[");
    for (int i = 0; i < count; i++) {
        if (i > 0) {
            buffer_append_char(&buf, ',');
        }
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

void api_list_handle_download(socket_t client) {
    char items[SHOPPING_LIST_MAX_ITEMS][SHOPPING_LIST_MAX_LEN];
    int count = load_shopping_list(items, SHOPPING_LIST_MAX_ITEMS);
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

static void handle_list_add_or_update(socket_t client, const HttpRequest *req, int is_update) {
    const char *artikel = find_param(req->body_params, req->body_count, "artikel");
    const char *anbieter = find_param(req->body_params, req->body_count, "anbieter");
    const char *index_text = find_param(req->body_params, req->body_count, "index");
    if (artikel == NULL || *artikel == '\0') {
        send_error(client, "400 Bad Request", "Artikel fehlt");
        return;
    }
    if (pruefe_text_eingabe(artikel, SHOPPING_LIST_MAX_LEN - 1) != 0) {
        send_error(client, "400 Bad Request", "Ungültiger Artikeltext");
        return;
    }
    if (pruefe_optionalen_text(anbieter, SHOPPING_LIST_MAX_LEN - 1) != 0) {
        send_error(client, "400 Bad Request", "Ungültiger Anbietertext");
        return;
    }
    char items[SHOPPING_LIST_MAX_ITEMS][SHOPPING_LIST_MAX_LEN];
    int count = load_shopping_list(items, SHOPPING_LIST_MAX_ITEMS);
    if (is_update) {
        if (index_text == NULL) {
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

void api_list_handle_add(socket_t client, const HttpRequest *req) {
    handle_list_add_or_update(client, req, 0);
}

void api_list_handle_update(socket_t client, const HttpRequest *req) {
    handle_list_add_or_update(client, req, 1);
}

void api_list_handle_delete(socket_t client, const HttpRequest *req) {
    const char *index_text = find_param(req->body_params, req->body_count, "index");
    if (index_text == NULL) {
        send_error(client, "400 Bad Request", "Index fehlt");
        return;
    }
    int index = 0;
    if (parse_int_param(index_text, &index) != 0) {
        send_error(client, "400 Bad Request", "Ungültiger Index");
        return;
    }
    char items[SHOPPING_LIST_MAX_ITEMS][SHOPPING_LIST_MAX_LEN];
    int count = load_shopping_list(items, SHOPPING_LIST_MAX_ITEMS);
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
