#include "crud_database.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

static void trim(char *s) {
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

static void read_line(char *buf, size_t size) {
    if (!buf || size == 0) return;
    if (!fgets(buf, (int)size, stdin)) {
        buf[0] = '\0';
        clearerr(stdin);
        return;
    }
    buf[strcspn(buf, "\r\n")] = '\0';
}

int load_database(const char *filepath, Database *db) {
    if (!filepath || !db) return -1;
    FILE *file = fopen(filepath, "r");
    if (!file) return -1;
    db->count = 0;
    strncpy(db->filename, filepath, DB_MAX_FILENAME - 1);
    db->filename[DB_MAX_FILENAME - 1] = '\0';
    char line[512];
    while (fgets(line, sizeof line, file)) {
        line[strcspn(line, "\r\n")] = '\0';
        if (line[0] == '\0') continue;
        char *fields[5];
        int field_count = 0;
        char *token = strtok(line, ",");
        while (token && field_count < 5) {
            fields[field_count++] = token;
            token = strtok(NULL, ",");
        }
        if (field_count != 5 || db->count >= DB_MAX_ENTRIES) continue;
        for (int i = 0; i < 5; i++) trim(fields[i]);
        char *endptr = NULL;
        long id = strtol(fields[0], &endptr, 10);
        if (!endptr || *endptr != '\0') continue;
        endptr = NULL;
        long preis = strtol(fields[3], &endptr, 10);
        if (!endptr || *endptr != '\0') continue;
        DatabaseEntry *entry = &db->entries[db->count++];
        entry->id = (int)id;
        strncpy(entry->artikel, fields[1], DB_MAX_TEXT - 1);
        entry->artikel[DB_MAX_TEXT - 1] = '\0';
        strncpy(entry->anbieter, fields[2], DB_MAX_TEXT - 1);
        entry->anbieter[DB_MAX_TEXT - 1] = '\0';
        entry->preis_ct = (int)preis;
        strncpy(entry->menge, fields[4], DB_MAX_TEXT - 1);
        entry->menge[DB_MAX_TEXT - 1] = '\0';
    }
    fclose(file);
    return 0;
}

int save_database(const Database *db) {
    if (!db) return -1;
    FILE *file = fopen(db->filename, "w");
    if (!file) return -1;
    for (int i = 0; i < db->count; i++) {
        const DatabaseEntry *entry = &db->entries[i];
        fprintf(file, "%d,%s,%s,%d,%s\n", entry->id, entry->artikel, entry->anbieter, entry->preis_ct, entry->menge);
    }
    fclose(file);
    return 0;
}

void print_database(const Database *db) {
    if (!db) return;
    printf("=============================================\n");
    printf("ID | Artikel | Anbieter | Preis(ct) | Menge\n");
    printf("=============================================\n");
    if (db->count == 0) {
        printf("Keine Einträge vorhanden.\n");
    } else {
        for (int i = 0; i < db->count; i++) {
            const DatabaseEntry *entry = &db->entries[i];
            printf("%d | %s | %s | %d | %s\n", entry->id, entry->artikel, entry->anbieter, entry->preis_ct, entry->menge);
        }
    }
    printf("=============================================\n");
}

int edit_database_entry(Database *db, int index) {
    if (!db || index < 0 || index >= db->count) return -1;
    DatabaseEntry *entry = &db->entries[index];
    char buffer[DB_MAX_TEXT];
    printf("ID (%d): ", entry->id);
    read_line(buffer, sizeof buffer);
    if (buffer[0] != '\0') {
        char *endptr = NULL;
        long v = strtol(buffer, &endptr, 10);
        if (endptr && *endptr == '\0') entry->id = (int)v;
    }
    printf("Artikel (%s): ", entry->artikel);
    read_line(buffer, sizeof buffer);
    if (buffer[0] != '\0') {
        strncpy(entry->artikel, buffer, DB_MAX_TEXT - 1);
        entry->artikel[DB_MAX_TEXT - 1] = '\0';
    }
    printf("Anbieter (%s): ", entry->anbieter);
    read_line(buffer, sizeof buffer);
    if (buffer[0] != '\0') {
        strncpy(entry->anbieter, buffer, DB_MAX_TEXT - 1);
        entry->anbieter[DB_MAX_TEXT - 1] = '\0';
    }
    printf("Preis in ct (%d): ", entry->preis_ct);
    read_line(buffer, sizeof buffer);
    if (buffer[0] != '\0') {
        char *endptr = NULL;
        long v = strtol(buffer, &endptr, 10);
        if (endptr && *endptr == '\0') entry->preis_ct = (int)v;
    }
    printf("Menge (%s): ", entry->menge);
    read_line(buffer, sizeof buffer);
    if (buffer[0] != '\0') {
        strncpy(entry->menge, buffer, DB_MAX_TEXT - 1);
        entry->menge[DB_MAX_TEXT - 1] = '\0';
    }
    return 0;
}

static int read_choice(const char *prompt) {
    char buffer[32];
    for (;;) {
        printf("%s", prompt);
        read_line(buffer, sizeof buffer);
        if (buffer[0] == '\0') continue;
        char *endptr = NULL;
        long v = strtol(buffer, &endptr, 10);
        if (endptr && *endptr == '\0') return (int)v;
    }
}

void edit_database(Database *db) {
    if (!db) return;
    int dirty = 0;
    for (;;) {
        printf("\n=== Datenbank: %s ===\n", db->filename);
        printf("[1] Einträge anzeigen\n");
        printf("[2] Eintrag bearbeiten\n");
        printf("[3] Änderungen speichern\n");
        printf("[4] Zurück\n");
        int choice = read_choice("Auswahl: ");
        switch (choice) {
            case 1:
                print_database(db);
                break;
            case 2:
                if (db->count == 0) {
                    printf("Keine Einträge vorhanden.\n");
                    break;
                }
                print_database(db);
                {
                    int index = read_choice("Eintragsnummer (1-basig): ");
                    if (index < 1 || index > db->count) {
                        printf("Ungültige Auswahl.\n");
                        break;
                    }
                    if (edit_database_entry(db, index - 1) == 0) dirty = 1;
                }
                break;
            case 3:
                if (save_database(db) == 0) {
                    dirty = 0;
                    printf("Gespeichert.\n");
                } else {
                    printf("Speichern fehlgeschlagen.\n");
                }
                break;
            case 4:
                if (dirty) {
                    printf("Es gibt ungespeicherte Änderungen. Trotzdem verlassen? (j/n): ");
                    char answer[8];
                    read_line(answer, sizeof answer);
                    if (answer[0] == 'j' || answer[0] == 'J') return;
                } else {
                    return;
                }
                break;
            default:
                printf("Ungültige Auswahl.\n");
                break;
        }
    }
}

static int has_csv_extension(const char *name) {
    size_t len = strlen(name);
    if (len < 4) return 0;
    return (tolower((unsigned char)name[len - 4]) == '.' &&
            tolower((unsigned char)name[len - 3]) == 'c' &&
            tolower((unsigned char)name[len - 2]) == 's' &&
            tolower((unsigned char)name[len - 1]) == 'v');
}

int list_csv_files(const char *directory, char files[][DB_MAX_FILENAME], int max_files) {
    if (!directory || !files || max_files <= 0) return -1;
    DIR *dir = opendir(directory);
    if (!dir) return -1;
    struct dirent *entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL && count < max_files) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) continue;
        if (!has_csv_extension(entry->d_name)) continue;
        char path[DB_MAX_FILENAME];
        snprintf(path, sizeof path, "%s/%s", directory, entry->d_name);
        struct stat st;
        if (stat(path, &st) == -1 || !S_ISREG(st.st_mode)) continue;
        strncpy(files[count], path, DB_MAX_FILENAME - 1);
        files[count][DB_MAX_FILENAME - 1] = '\0';
        count++;
    }
    closedir(dir);
    return count;
}
