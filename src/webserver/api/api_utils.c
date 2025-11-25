#include "webserver/api/api_utils.h"

#include "database/database_controller.h"
#include "database/text_input_utils.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
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

int resolve_database_path(const char *name, char *out, size_t out_size) {
    if (name == NULL || *name == '\0') {
        return -1;
    }
    char files[MAX_DB_FILES][DB_MAX_FILENAME];
    int count = list_csv_files(DATA_DIRECTORY, files, MAX_DB_FILES);
    if (count <= 0) {
        return -1;
    }
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

int load_shopping_list(char items[][SHOPPING_LIST_MAX_LEN], int max_items) {
    FILE *file = fopen(SHOPPING_LIST_PATH, "r");
    if (file == NULL) {
        return 0;
    }
    int count = 0;
    while (count < max_items && fgets(items[count], sizeof items[count], file)) {
        items[count][strcspn(items[count], "\r\n")] = '\0';
        count++;
    }
    fclose(file);
    return count;
}

int save_shopping_list(char items[][SHOPPING_LIST_MAX_LEN], int count) {
    FILE *file = fopen(SHOPPING_LIST_PATH, "w");
    if (file == NULL) {
        return -1;
    }
    for (int i = 0; i < count; i++) {
        fprintf(file, "%.*s\n", SHOPPING_LIST_MAX_LEN - 1, items[i]);
    }
    fclose(file);
    return 0;
}

void trim_whitespace_inplace(char *text) {
    if (text == NULL) {
        return;
    }

    size_t len = strlen(text);
    while (len > 0 && isspace((unsigned char)text[len - 1])) {
        text[--len] = '\0';
    }

    size_t start = 0;
    while (text[start] != '\0' && isspace((unsigned char)text[start])) {
        start++;
    }
    if (start > 0) {
        memmove(text, text + start, strlen(text + start) + 1);
    }
}

void split_list_entry(const char *line, char *article, size_t article_size, char *provider, size_t provider_size) {
    if (line == NULL) {
        if (article != NULL && article_size > 0) {
            article[0] = '\0';
        }
        if (provider != NULL && provider_size > 0) {
            provider[0] = '\0';
        }
        return;
    }

    const char *sep = strchr(line, '|');
    if (sep != NULL) {
        size_t article_len = (size_t)(sep - line);
        if (article_size > 0) {
            if (article_len >= article_size) {
                article_len = article_size - 1;
            }
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
        if (provider_size > 0) {
            provider[0] = '\0';
        }
    }

    trim_whitespace_inplace(article);
    trim_whitespace_inplace(provider);
}

static int unit_normalize(const char *unit, UnitType *type, double *factor) {
    if (unit == NULL || type == NULL || factor == NULL) {
        return -1;
    }
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

int entry_to_quantity(const DatabaseEntry *entry, Quantity *quantity) {
    if (entry == NULL || quantity == NULL) {
        return -1;
    }
    if (entry->menge_wert <= 0.0) {
        return -1;
    }
    UnitType type = UNIT_UNKNOWN;
    double factor = 0.0;
    if (unit_normalize(entry->menge_einheit, &type, &factor) != 0) {
        return -1;
    }
    quantity->amount = entry->menge_wert * factor;
    quantity->type = type;
    return 0;
}

const char *unit_label(UnitType type) {
    switch (type) {
        case UNIT_GRAM: return "g";
        case UNIT_MILLILITER: return "ml";
        case UNIT_PIECE: return "Stück";
        default: return "";
    }
}

void build_list_entry(const char *artikel, const char *anbieter, char *out, size_t out_size) {
    if (anbieter != NULL && *anbieter != '\0') {
        snprintf(out, out_size, "%s|%s", artikel, anbieter);
    } else {
        snprintf(out, out_size, "%s", artikel);
    }
    out[out_size - 1] = '\0';
}

int parse_int_param(const char *value, int *out) {
    if (value == NULL) {
        return -1;
    }
    if (pruefe_ganzzahl_eingabe(value, LONG_MIN, LONG_MAX) != 0) {
        return -1;
    }
    char *end = NULL;
    long v = strtol(value, &end, 10);
    if (end == NULL || *end != '\0') {
        return -1;
    }
    *out = (int)v;
    return 0;
}

DatabaseEntry *find_entry_by_id(Database *db, int id) {
    if (db == NULL) {
        return NULL;
    }
    for (int i = 0; i < db->anzahl; i++) {
        if (db->eintraege[i].id == id) {
            return &db->eintraege[i];
        }
    }
    return NULL;
}

int find_entry_index(const Database *db, int id) {
    if (db == NULL) {
        return -1;
    }
    for (int i = 0; i < db->anzahl; i++) {
        if (db->eintraege[i].id == id) {
            return i;
        }
    }
    return -1;
}
