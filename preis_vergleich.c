#include "preis_vergleich.h"

#include "crud_database.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FILES 128
#define LIST_MAX_ITEMS 100
#define LIST_MAX_LEN 256
#define SHOPPING_LIST_FILE "einkaufsliste.txt"

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

static void read_line(char *buf, size_t size) {
    if (!buf || size == 0) return;
    if (!fgets(buf, (int)size, stdin)) {
        buf[0] = '\0';
        clearerr(stdin);
        return;
    }
    buf[strcspn(buf, "\r\n")] = '\0';
}

static int read_int(const char *prompt) {
    char buffer[64];
    for (;;) {
        printf("%s", prompt);
        read_line(buffer, sizeof buffer);
        if (buffer[0] == '\0') continue;
        char *end = NULL;
        long value = strtol(buffer, &end, 10);
        if (end && *end == '\0') return (int)value;
        printf("Ungültige Eingabe.\n");
    }
}

static const char *basename_of(const char *path) {
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *backslash = strrchr(path, '\\');
    if (!slash || (backslash && backslash > slash)) slash = backslash;
#endif
    if (!slash) return path;
    return slash + 1;
}

static const char *unit_label(UnitType type) {
    switch (type) {
        case UNIT_GRAM: return "g";
        case UNIT_MILLILITER: return "ml";
        case UNIT_PIECE: return "Stück";
        default: return "";
    }
}

static int normalize_unit(const char *unit, UnitType *type, double *factor) {
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
    if (strcmp(unit, "ml") == 0) {
        *type = UNIT_MILLILITER;
        *factor = 1.0;
        return 0;
    }
    if (strcmp(unit, "l") == 0) {
        *type = UNIT_MILLILITER;
        *factor = 1000.0;
        return 0;
    }
    if (strcmp(unit, "stk") == 0 || strcmp(unit, "st") == 0 || strcmp(unit, "stück") == 0) {
        *type = UNIT_PIECE;
        *factor = 1.0;
        return 0;
    }
    return -1;
}

static int parse_quantity_text(const char *text, Quantity *quantity) {
    if (!text || !quantity) return -1;
    char buffer[DB_MAX_TEXT];
    strncpy(buffer, text, sizeof buffer - 1);
    buffer[sizeof buffer - 1] = '\0';
    char *p = buffer;
    while (*p && isspace((unsigned char)*p)) p++;
    for (char *c = p; *c; ++c) {
        if (*c == ',') *c = '.';
    }
    char *end = NULL;
    double value = strtod(p, &end);
    if (end == p) return -1;
    while (*end && isspace((unsigned char)*end)) end++;
    if (*end == '\0') return -1;
    char unit_buf[16];
    size_t idx = 0;
    while (*end && idx + 1 < sizeof unit_buf) {
        unit_buf[idx++] = (char)tolower((unsigned char)*end);
        end++;
    }
    unit_buf[idx] = '\0';
    UnitType type = UNIT_UNKNOWN;
    double factor = 0.0;
    if (normalize_unit(unit_buf, &type, &factor) != 0) return -1;
    quantity->amount = value * factor;
    quantity->type = type;
    return 0;
}

static int select_database_path(const char *directory, char *out_path, size_t out_size) {
    char files[MAX_FILES][DB_MAX_FILENAME];
    int count = list_csv_files(directory, files, MAX_FILES);
    if (count <= 0) {
        printf("Keine Datenbanken gefunden.\n");
        return -1;
    }
    printf("Verfügbare Datenbanken:\n");
    for (int i = 0; i < count; i++) {
        printf("[%d] %s\n", i + 1, basename_of(files[i]));
    }
    for (;;) {
        int choice = read_int("Auswahl: ");
        if (choice >= 1 && choice <= count) {
            strncpy(out_path, files[choice - 1], out_size - 1);
            out_path[out_size - 1] = '\0';
            return 0;
        }
        printf("Ungültige Auswahl.\n");
    }
}

static DatabaseEntry *find_entry_by_id(Database *db, int id) {
    if (!db) return NULL;
    for (int i = 0; i < db->count; i++) {
        if (db->entries[i].id == id) return &db->entries[i];
    }
    return NULL;
}

static DatabaseEntry *select_entry(Database *db, const char *prompt, int exclude_id) {
    for (;;) {
        int id = read_int(prompt);
        if (exclude_id != -1 && id == exclude_id) {
            printf("Dieser Eintrag wurde bereits gewählt.\n");
            continue;
        }
        DatabaseEntry *entry = find_entry_by_id(db, id);
        if (entry) return entry;
        printf("ID nicht gefunden.\n");
    }
}

static double read_amount(UnitType type) {
    char buffer[64];
    const char *unit = unit_label(type);
    for (;;) {
        printf("Gewünschte Menge in %s: ", unit);
        read_line(buffer, sizeof buffer);
        if (buffer[0] == '\0') continue;
        for (char *c = buffer; *c; ++c) {
            if (*c == ',') *c = '.';
        }
        char *end = NULL;
        double value = strtod(buffer, &end);
        if (end && *end == '\0' && value > 0.0) {
            return value;
        }
        printf("Ungültige Eingabe.\n");
    }
}

static void trim_spaces(char *s) {
    if (!s) return;
    char *start = s;
    while (*start && isspace((unsigned char)*start)) start++;
    if (start != s) memmove(s, start, strlen(start) + 1);
    size_t len = strlen(s);
    while (len > 0 && isspace((unsigned char)s[len - 1])) {
        s[len - 1] = '\0';
        len--;
    }
}

static void split_list_entry(const char *line, char *article, size_t article_size, char *provider, size_t provider_size) {
    if (!line) {
        if (article && article_size > 0) article[0] = '\0';
        if (provider && provider_size > 0) provider[0] = '\0';
        return;
    }
    const char *sep = strchr(line, '|');
    if (sep) {
        size_t len = (size_t)(sep - line);
        if (article && article_size > 0) {
            if (len >= article_size) len = article_size - 1;
            strncpy(article, line, len);
            article[len] = '\0';
        }
        if (provider && provider_size > 0) {
            strncpy(provider, sep + 1, provider_size - 1);
            provider[provider_size - 1] = '\0';
        }
    } else {
        if (article && article_size > 0) {
            strncpy(article, line, article_size - 1);
            article[article_size - 1] = '\0';
        }
        if (provider && provider_size > 0) provider[0] = '\0';
    }
    if (article) trim_spaces(article);
    if (provider) trim_spaces(provider);
}

static int load_shopping_list(const char *filename, char items[LIST_MAX_ITEMS][LIST_MAX_LEN]) {
    FILE *file = fopen(filename, "r");
    if (!file) return 0;
    int count = 0;
    while (count < LIST_MAX_ITEMS && fgets(items[count], LIST_MAX_LEN, file)) {
        items[count][strcspn(items[count], "\r\n")] = '\0';
        count++;
    }
    fclose(file);
    return count;
}

static void save_shopping_list(const char *filename, char items[LIST_MAX_ITEMS][LIST_MAX_LEN], int count) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        printf("Einkaufsliste konnte nicht gespeichert werden.\n");
        return;
    }
    for (int i = 0; i < count; i++) {
        fprintf(file, "%s\n", items[i]);
    }
    fclose(file);
}

static void compare_single(Database *db) {
    if (!db) return;
    if (db->count < 2) {
        printf("Zu wenige Einträge für einen Vergleich.\n");
        return;
    }
    print_database(db);
    DatabaseEntry *first = select_entry(db, "ID des ersten Eintrags: ", -1);
    DatabaseEntry *second = select_entry(db, "ID des zweiten Eintrags: ", first->id);
    Quantity qty_first;
    Quantity qty_second;
    if (parse_quantity_text(first->menge, &qty_first) != 0 ||
        parse_quantity_text(second->menge, &qty_second) != 0) {
        printf("Mengenangaben konnten nicht interpretiert werden.\n");
        return;
    }
    if (qty_first.type != qty_second.type) {
        printf("Die Einheiten der ausgewählten Produkte stimmen nicht überein.\n");
        return;
    }
    double desired_amount = read_amount(qty_first.type);
    double unit_price_first = (double)first->preis_ct / qty_first.amount;
    double unit_price_second = (double)second->preis_ct / qty_second.amount;
    double total_first = unit_price_first * desired_amount;
    double total_second = unit_price_second * desired_amount;
    const char *unit = unit_label(qty_first.type);
    printf("\nVergleich der Produkte für %.2f %s:\n", desired_amount, unit);
    printf("1) %s (%s)\n", first->artikel, first->anbieter);
    printf("   Menge pro Packung: %s\n", first->menge);
    printf("   Preis pro Packung: %d ct (%.2f €)\n", first->preis_ct, first->preis_ct / 100.0);
    printf("   Preis pro %s: %.4f ct\n", unit, unit_price_first);
    printf("   Preis für %.2f %s: %.2f € (%.2f ct)\n\n", desired_amount, unit, total_first / 100.0, total_first);
    printf("2) %s (%s)\n", second->artikel, second->anbieter);
    printf("   Menge pro Packung: %s\n", second->menge);
    printf("   Preis pro Packung: %d ct (%.2f €)\n", second->preis_ct, second->preis_ct / 100.0);
    printf("   Preis pro %s: %.4f ct\n", unit, unit_price_second);
    printf("   Preis für %.2f %s: %.2f € (%.2f ct)\n", desired_amount, unit, total_second / 100.0, total_second);
    const double epsilon = 1e-6;
    if (unit_price_first + epsilon < unit_price_second) {
        printf("\nGünstiger pro %s ist: %s (%s)\n", unit, first->artikel, first->anbieter);
    } else if (unit_price_second + epsilon < unit_price_first) {
        printf("\nGünstiger pro %s ist: %s (%s)\n", unit, second->artikel, second->anbieter);
    } else {
        printf("\nBeide Produkte sind pro %s gleich teuer.\n", unit);
    }
}

static void compare_shopping_list(Database *db, const char *list_file) {
    if (!db || !list_file) return;
    char items[LIST_MAX_ITEMS][LIST_MAX_LEN];
    int count = load_shopping_list(list_file, items);
    if (count == 0) {
        printf("Die Einkaufsliste ist leer.\n");
        return;
    }
    int best_indices[LIST_MAX_ITEMS];
    for (int i = 0; i < count; i++) best_indices[i] = -1;
    printf("\nPreisvergleich für die Einkaufsliste:\n");
    for (int i = 0; i < count; i++) {
        char article[DB_MAX_TEXT];
        char provider[DB_MAX_TEXT];
        split_list_entry(items[i], article, sizeof article, provider, sizeof provider);
        if (article[0] == '\0') {
            printf("- Position %d konnte nicht gelesen werden.\n", i + 1);
            continue;
        }
        printf("\n%s:\n", article);
        int matches = 0;
        int best_idx = -1;
        int best_has_qty = 0;
        double best_unit_price = 0.0;
        int best_pack_price = 0;
        for (int j = 0; j < db->count; j++) {
            DatabaseEntry *entry = &db->entries[j];
            if (strcmp(entry->artikel, article) != 0) continue;
            matches++;
            Quantity qty;
            int has_qty = 0;
            double unit_price = 0.0;
            if (parse_quantity_text(entry->menge, &qty) == 0 && qty.amount > 0.0) {
                has_qty = 1;
                unit_price = (double)entry->preis_ct / qty.amount;
            }
            printf("  - %s: %d ct (%.2f €) pro Packung, %s", entry->anbieter, entry->preis_ct, entry->preis_ct / 100.0, entry->menge);
            if (has_qty) {
                printf(", %.4f ct pro %s", unit_price, unit_label(qty.type));
            }
            if (provider[0] != '\0' && strcmp(provider, entry->anbieter) == 0) {
                printf(" [aktuelle Auswahl]");
            }
            printf("\n");
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
        if (matches == 0) {
            printf("  Kein Anbieter gefunden.\n");
            continue;
        }
        if (best_idx >= 0) {
            DatabaseEntry *best = &db->entries[best_idx];
            printf("  -> Günstigster Anbieter: %s (%s) mit %d ct pro Packung\n", best->anbieter, best->menge, best->preis_ct);
            best_indices[i] = best_idx;
        }
    }
    printf("\nErgebnisse in die Einkaufsliste übernehmen? (j/n): ");
    char answer[8];
    read_line(answer, sizeof answer);
    if (answer[0] == 'j' || answer[0] == 'J') {
        for (int i = 0; i < count; i++) {
            if (best_indices[i] >= 0) {
                DatabaseEntry *best = &db->entries[best_indices[i]];
                snprintf(items[i], LIST_MAX_LEN, "%s|%s", best->artikel, best->anbieter);
            }
        }
        save_shopping_list(list_file, items, count);
        printf("Einkaufsliste aktualisiert.\n");
    }
}

void compare_prices_menu(const char *directory) {
    if (!directory || directory[0] == '\0') directory = DATA_DIRECTORY;
    printf("[1] Einzelvergleich\n");
    printf("[2] Einkaufszettel vergleichen\n");
    int mode = read_int("Auswahl: ");
    if (mode != 1 && mode != 2) {
        printf("Ungültige Auswahl.\n");
        return;
    }
    char path[DB_MAX_FILENAME];
    if (select_database_path(directory, path, sizeof path) != 0) return;
    Database db;
    if (load_database(path, &db) != 0) {
        printf("Datei konnte nicht geladen werden.\n");
        return;
    }
    if (mode == 1) {
        compare_single(&db);
    } else {
        compare_shopping_list(&db, SHOPPING_LIST_FILE);
    }
}
