#include "webserver/api/api_db_handlers.h"

#include "database/database_controller.h"
#include "database/quantity_unit_utils.h"
#include "database/text_input_utils.h"

#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DB_FILES 128

static const char *basename_of(const char *path) {
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *backslash = strrchr(path, '\\');
    if (slash == NULL || (backslash != NULL && backslash > slash)) {
        slash = backslash;
    }
#endif
    return slash ? slash + 1 : path;
}

static int next_entry_id(const Database *db) {
    int max_id = 0;
    for (int i = 0; i < db->anzahl; i++) {
        if (db->eintraege[i].id > max_id) {
            max_id = db->eintraege[i].id;
        }
    }
    return max_id + 1;
}

static void append_menge_json(Buffer *buf, const DatabaseEntry *entry) {
    buffer_append_str(buf, ",\"mengeWert\":");
    char mengen_text[32];
    formatiere_mengenwert(entry->menge_wert, mengen_text, sizeof mengen_text);
    buffer_append_format(buf, "%s", mengen_text);
    buffer_append_str(buf, ",\"mengeEinheit\":");
    append_json_string(buf, entry->menge_einheit);
}

void api_db_handle_files(socket_t client) {
    char files[MAX_DB_FILES][DB_MAX_FILENAME];
    int count = list_csv_files(DATA_DIRECTORY, files, MAX_DB_FILES);
    Buffer buf;
    if (buffer_init(&buf) != 0) {
        send_error(client, "500 Internal Server Error", "Speicherfehler");
        return;
    }
    buffer_append_str(&buf, "{\"files\":[");
    for (int i = 0; i < count; i++) {
        if (i > 0) {
            buffer_append_char(&buf, ',');
        }
        buffer_append_str(&buf, "{\"name\":");
        append_json_string(&buf, basename_of(files[i]));
        buffer_append_char(&buf, '}');
    }
    buffer_append_str(&buf, "]}");
    send_json_response(client, "200 OK", buf.data, buf.len);
    buffer_free(&buf);
}

void api_db_handle_get(socket_t client, const HttpRequest *req) {
    const char *name = find_param(req->query_params, req->query_count, "name");
    if (name == NULL || *name == '\0') {
        send_error(client, "400 Bad Request", "Parameter 'name' fehlt");
        return;
    }
    if (pruefe_dateiname(name, DB_MAX_FILENAME - 1) != 0) {
        send_error(client, "400 Bad Request", "Ungültiger Datenbankname");
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
        if (i > 0) {
            buffer_append_char(&buf, ',');
        }
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

static void handle_db_add_or_update(socket_t client, const HttpRequest *req, int is_update) {
    const char *name = find_param(req->body_params, req->body_count, "name");
    const char *artikel = find_param(req->body_params, req->body_count, "artikel");
    const char *anbieter = find_param(req->body_params, req->body_count, "anbieter");
    const char *preis_text = find_param(req->body_params, req->body_count, "preisCent");
    const char *menge_wert_text = find_param(req->body_params, req->body_count, "mengeWert");
    const char *menge_einheit = find_param(req->body_params, req->body_count, "mengeEinheit");
    const char *id_text = find_param(req->body_params, req->body_count, "id");

    if (name == NULL || artikel == NULL || anbieter == NULL || preis_text == NULL ||
        menge_wert_text == NULL || menge_einheit == NULL) {
        send_error(client, "400 Bad Request", "Parameter fehlen");
        return;
    }
    if (pruefe_dateiname(name, DB_MAX_FILENAME - 1) != 0) {
        send_error(client, "400 Bad Request", "Ungültiger Datenbankname");
        return;
    }
    if (pruefe_text_eingabe(artikel, DB_MAX_TEXT - 1) != 0 ||
        pruefe_text_eingabe(anbieter, DB_MAX_TEXT - 1) != 0 ||
        pruefe_text_eingabe(menge_einheit, DB_MAX_TEXT - 1) != 0) {
        send_error(client, "400 Bad Request", "Ungültige Texteingabe");
        return;
    }
    if (pruefe_dezimalzahl_eingabe(menge_wert_text, 0.000001, 1e9, 6) != 0) {
        send_error(client, "400 Bad Request", "Ungültige Menge");
        return;
    }
    if (pruefe_ganzzahl_eingabe(preis_text, 0, INT_MAX) != 0) {
        send_error(client, "400 Bad Request", "Ungültiger Preis");
        return;
    }

    int preis_cent = atoi(preis_text);
    double menge_wert = atof(menge_wert_text);

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
    if (is_update) {
        int id = 0;
        if (parse_int_param(id_text, &id) != 0 || id < 0) {
            send_error(client, "400 Bad Request", "Ungültige ID");
            return;
        }
        entry = find_entry_by_id(&db, id);
        if (entry == NULL) {
            send_error(client, "404 Not Found", "Eintrag nicht gefunden");
            return;
        }
        entry->id = id;
    } else {
        entry = &db.eintraege[db.anzahl];
        entry->id = next_entry_id(&db);
    }
    strncpy(entry->artikel, artikel, DB_MAX_TEXT - 1);
    entry->artikel[DB_MAX_TEXT - 1] = '\0';
    strncpy(entry->anbieter, anbieter, DB_MAX_TEXT - 1);
    entry->anbieter[DB_MAX_TEXT - 1] = '\0';
    entry->preis_ct = preis_cent;
    entry->menge_wert = menge_wert;
    strncpy(entry->menge_einheit, menge_einheit, sizeof entry->menge_einheit - 1);
    entry->menge_einheit[sizeof entry->menge_einheit - 1] = '\0';

    if (!is_update) {
        db.anzahl++;
    }
    if (save_database(&db) != 0) {
        send_error(client, "500 Internal Server Error", "Speichern fehlgeschlagen");
        return;
    }
    send_json_string(client, "200 OK", "{\"status\":\"ok\"}");
}

void api_db_handle_add(socket_t client, const HttpRequest *req) {
    handle_db_add_or_update(client, req, 0);
}

void api_db_handle_update(socket_t client, const HttpRequest *req) {
    handle_db_add_or_update(client, req, 1);
}

void api_db_handle_delete(socket_t client, const HttpRequest *req) {
    const char *name = find_param(req->body_params, req->body_count, "name");
    const char *id_text = find_param(req->body_params, req->body_count, "id");
    if (name == NULL || id_text == NULL) {
        send_error(client, "400 Bad Request", "Parameter fehlen");
        return;
    }
    if (pruefe_dateiname(name, DB_MAX_FILENAME - 1) != 0) {
        send_error(client, "400 Bad Request", "Ungültiger Datenbankname");
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
    int index = find_entry_index(&db, id);
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
