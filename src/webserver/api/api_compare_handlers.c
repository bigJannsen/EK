#include "webserver/api/api_compare_handlers.h"

#include "database/database_controller.h"
#include "database/quantity_unit_utils.h"
#include "database/text_input_utils.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static void append_menge_json(Buffer *buf, const DatabaseEntry *entry) {
    buffer_append_str(buf, ",\"mengeWert\":");
    char mengen_text[32];
    formatiere_mengenwert(entry->menge_wert, mengen_text, sizeof mengen_text);
    buffer_append_format(buf, "%s", mengen_text);
    buffer_append_str(buf, ",\"mengeEinheit\":");
    append_json_string(buf, entry->menge_einheit);
}

static int parse_double_param(const char *value, double *out) {
    if (value == NULL) {
        return -1;
    }
    if (pruefe_dezimalzahl_eingabe(value, 0.000001, 1e9, 6) != 0) {
        return -1;
    }
    char *end = NULL;
    double v = strtod(value, &end);
    if (end == NULL || *end != '\0') {
        return -1;
    }
    *out = v;
    return 0;
}

static int find_list_provider(const Database *db, const char *article, const char *provider) {
    for (int i = 0; i < db->anzahl; i++) {
        if (strcmp(db->eintraege[i].artikel, article) == 0) {
            if (provider == NULL || provider[0] == '\0' || strcmp(db->eintraege[i].anbieter, provider) == 0) {
                return i;
            }
        }
    }
    return -1;
}

void api_compare_handle_single(socket_t client, const HttpRequest *req) {
    const char *name = find_param(req->body_params, req->body_count, "name");
    const char *first_id_text = find_param(req->body_params, req->body_count, "firstId");
    const char *second_id_text = find_param(req->body_params, req->body_count, "secondId");
    const char *amount_text = find_param(req->body_params, req->body_count, "amount");
    if (name == NULL || first_id_text == NULL || second_id_text == NULL || amount_text == NULL) {
        send_error(client, "400 Bad Request", "Parameter fehlen");
        return;
    }
    if (pruefe_dateiname(name, DB_MAX_FILENAME - 1) != 0) {
        send_error(client, "400 Bad Request", "Ungültiger Datenbankname");
        return;
    }
    int first_id = 0;
    int second_id = 0;
    double amount = 0.0;
    if (parse_int_param(first_id_text, &first_id) != 0 || parse_int_param(second_id_text, &second_id) != 0) {
        send_error(client, "400 Bad Request", "Ungültige IDs");
        return;
    }
    if (first_id < 0 || second_id < 0) {
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
    DatabaseEntry *first = find_entry_by_id(&db, first_id);
    DatabaseEntry *second = find_entry_by_id(&db, second_id);
    if (first == NULL || second == NULL) {
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
    if (unit_price_first + epsilon < unit_price_second) {
        cheaper = "first";
    } else if (unit_price_second + epsilon < unit_price_first) {
        cheaper = "second";
    }
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
    buffer_append_char(&buf, '}');
    send_json_response(client, "200 OK", buf.data, buf.len);
    buffer_free(&buf);
}

static int find_best_price(const Database *db, const char *article, const char *preferred_provider, int *best_index) {
    double best_price = INFINITY;
    int found = 0;
    for (int i = 0; i < db->anzahl; i++) {
        if (strcmp(db->eintraege[i].artikel, article) != 0) {
            continue;
        }
        Quantity qty;
        if (entry_to_quantity(&db->eintraege[i], &qty) != 0 || qty.amount <= 0) {
            continue;
        }
        double unit_price = (double)db->eintraege[i].preis_ct / qty.amount;
        if (preferred_provider != NULL && preferred_provider[0] != '\0' &&
            strcmp(db->eintraege[i].anbieter, preferred_provider) != 0) {
            if (!isfinite(best_price)) {
                best_price = unit_price;
                *best_index = i;
                found = 1;
            }
            continue;
        }
        if (unit_price < best_price) {
            best_price = unit_price;
            *best_index = i;
            found = 1;
        }
    }
    return found ? 0 : -1;
}

void api_compare_handle_list(socket_t client, const HttpRequest *req) {
    const char *name = find_param(req->body_params, req->body_count, "name");
    const char *apply_text = find_param(req->body_params, req->body_count, "apply");
    if (name == NULL) {
        send_error(client, "400 Bad Request", "Parameter fehlen");
        return;
    }
    if (pruefe_dateiname(name, DB_MAX_FILENAME - 1) != 0) {
        send_error(client, "400 Bad Request", "Ungültiger Datenbankname");
        return;
    }
    int apply = 0;
    if (apply_text != NULL && strcmp(apply_text, "1") == 0) {
        apply = 1;
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
    int count = load_shopping_list(items, SHOPPING_LIST_MAX_ITEMS);
    Buffer buf;
    if (buffer_init(&buf) != 0) {
        send_error(client, "500 Internal Server Error", "Speicherfehler");
        return;
    }
    int changed = 0;
    buffer_append_str(&buf, "{\"status\":\"ok\",\"items\":[");
    for (int i = 0; i < count; i++) {
        if (i > 0) {
            buffer_append_char(&buf, ',');
        }
        char article[DB_MAX_TEXT];
        char provider[DB_MAX_TEXT];
        split_list_entry(items[i], article, sizeof article, provider, sizeof provider);
        buffer_append_str(&buf, "{\"text\":");
        append_json_string(&buf, items[i]);
        buffer_append_str(&buf, ",\"empfehlung\":null");
        int idx = find_list_provider(&db, article, provider);
        if (idx >= 0) {
            buffer_append_str(&buf, ",\"treffer\":{");
            buffer_append_str(&buf, "\"anbieter\":");
            append_json_string(&buf, db.eintraege[idx].anbieter);
            append_menge_json(&buf, &db.eintraege[idx]);
            buffer_append_str(&buf, ",\"preisCent\":");
            buffer_append_format(&buf, "%d", db.eintraege[idx].preis_ct);
            Quantity qty;
            if (entry_to_quantity(&db.eintraege[idx], &qty) == 0 && qty.amount > 0.0) {
                buffer_append_str(&buf, ",\"unitPrice\":");
                buffer_append_format(&buf, "%.6f", (double)db.eintraege[idx].preis_ct / qty.amount);
                buffer_append_str(&buf, ",\"unit\":");
                append_json_string(&buf, unit_label(qty.type));
            }
            buffer_append_char(&buf, '}');
        }
        int best = -1;
        if (find_best_price(&db, article, provider, &best) == 0) {
            DatabaseEntry *best_entry = &db.eintraege[best];
            buffer_append_str(&buf, ",\"empfehlung\":{");
            buffer_append_str(&buf, "\"anbieter\":");
            append_json_string(&buf, best_entry->anbieter);
            append_menge_json(&buf, best_entry);
            buffer_append_str(&buf, ",\"preisCent\":");
            buffer_append_format(&buf, "%d", best_entry->preis_ct);
            Quantity best_qty;
            if (entry_to_quantity(best_entry, &best_qty) == 0 && best_qty.amount > 0.0) {
                buffer_append_str(&buf, ",\"unitPrice\":");
                buffer_append_format(&buf, "%.6f", (double)best_entry->preis_ct / best_qty.amount);
                buffer_append_str(&buf, ",\"unit\":");
                append_json_string(&buf, unit_label(best_qty.type));
            }
            buffer_append_char(&buf, '}');
            if (apply != 0 && strcmp(provider, best_entry->anbieter) != 0) {
                changed = 1;
                build_list_entry(best_entry->artikel, best_entry->anbieter, items[i], sizeof items[i]);
            }
        }
        buffer_append_char(&buf, '}');
    }
    buffer_append_str(&buf, "]}");
    if (apply != 0 && changed != 0) {
        save_shopping_list(items, count);
    }
    send_json_response(client, "200 OK", buf.data, buf.len);
    buffer_free(&buf);
}
