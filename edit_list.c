#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "edit_list.h"
#include "crud_database.h"
#include "edit_data.h"

#define MAX_ITEMS 100
#define MAX_LEN 256
#define MAX_FILES 128

// --- Hilfsfunktion: Bildschirm löschen ---
static void clear_screen() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// --- Liest vorhandene Liste ein ---
static int load_list(const char *filename, char items[MAX_ITEMS][MAX_LEN]) {
    FILE *file = fopen(filename, "r");
    int count = 0;

    if (file == NULL) {
        return 0; // Datei existiert noch nicht → leere Liste
    }

    while (fgets(items[count], MAX_LEN, file) && count < MAX_ITEMS) {
        items[count][strcspn(items[count], "\r\n")] = '\0'; // Zeilenende entfernen
        count++;
    }

    fclose(file);
    return count;
}

// --- Speichert Liste ---
static void save_list(const char *filename, char items[MAX_ITEMS][MAX_LEN], int count) {
    FILE *file = fopen(filename, "w");
    if (!file) {
        perror("Fehler beim Speichern der Datei");
        return;
    }

    for (int i = 0; i < count; i++) {
        fprintf(file, "%s\n", items[i]);
    }
    fclose(file);
}

// --- Zeigt Liste an ---
static void print_list(char items[MAX_ITEMS][MAX_LEN], int count) {
    printf("=========================================\n");
    printf("          Aktuelle Einkaufsliste\n");
    printf("=========================================\n");
    if (count == 0) {
        printf("(Die Liste ist leer.)\n");
    } else {
        for (int i = 0; i < count; i++) {
            const char *sep = strchr(items[i], '|');
            if (sep) {
                size_t article_len = (size_t)(sep - items[i]);
                char article[MAX_LEN];
                if (article_len >= sizeof article) article_len = sizeof article - 1;
                strncpy(article, items[i], article_len);
                article[article_len] = '\0';
                const char *provider = sep + 1;
                printf("%2d | %s (%s)\n", i + 1, article, provider);
            } else {
                printf("%2d | %s\n", i + 1, items[i]);
            }
        }
    }
    printf("=========================================\n");
}

// --- Artikel hinzufügen ---
static void flush_input(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
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

static int select_database(Database *db) {
    char files[MAX_FILES][DB_MAX_FILENAME];
    int count = list_csv_files(DATA_DIRECTORY, files, MAX_FILES);
    if (count <= 0) {
        printf("Keine Datenbank gefunden.\n");
        printf("\nWeiter mit Enter ...");
        getchar();
        return -1;
    }
    printf("Verfügbare Datenbanken:\n");
    for (int i = 0; i < count; i++) {
        printf("[%d] %s\n", i + 1, basename_of(files[i]));
    }
    printf("Auswahl: ");
    int choice;
    if (scanf("%d", &choice) != 1 || choice < 1 || choice > count) {
        printf("Ungültige Auswahl.\n");
        flush_input();
        printf("\nWeiter mit Enter ...");
        getchar();
        return -1;
    }
    flush_input();
    if (load_database(files[choice - 1], db) != 0) {
        printf("Datei konnte nicht geladen werden.\n");
        printf("\nWeiter mit Enter ...");
        getchar();
        return -1;
    }
    return 0;
}

static DatabaseEntry *find_entry(Database *db, int id) {
    if (!db) return NULL;
    for (int i = 0; i < db->count; i++) {
        if (db->entries[i].id == id) return &db->entries[i];
    }
    return NULL;
}

static void add_item(Database *db, char items[MAX_ITEMS][MAX_LEN], int *count) {
    if (*count >= MAX_ITEMS) {
        printf("Die Liste ist voll! (max. %d Einträge)\n", MAX_ITEMS);
        printf("\nWeiter mit Enter ...");
        getchar(); getchar();
        return;
    }
    if (!db || db->count == 0) {
        printf("Keine Datenbank geladen.\n");
        printf("\nWeiter mit Enter ...");
        getchar(); getchar();
        return;
    }
    print_database(db);
    printf("ID des Artikels: ");
    int id;
    if (scanf("%d", &id) != 1) {
        printf("Ungültige Eingabe!\n");
        flush_input();
        printf("\nWeiter mit Enter ...");
        getchar(); getchar();
        return;
    }
    flush_input();
    DatabaseEntry *entry = find_entry(db, id);
    if (!entry) {
        printf("ID nicht gefunden.\n");
        printf("\nWeiter mit Enter ...");
        getchar(); getchar();
        return;
    }
    snprintf(items[*count], MAX_LEN, "%s|%s", entry->artikel, entry->anbieter);
    (*count)++;
    printf("\nArtikel hinzugefügt!\n");
    printf("\nWeiter mit Enter ...");
    getchar();
}

// --- Artikel löschen ---
static void remove_item(char items[MAX_ITEMS][MAX_LEN], int *count) {
    if (*count == 0) {
        printf("Die Liste ist leer, nichts zu löschen.\n");
        printf("\nWeiter mit Enter ...");
        getchar(); getchar();
        return;
    }

    print_list(items, *count);

    printf("Nummer des zu löschenden Artikels: ");
    int num;
    if (scanf("%d", &num) != 1 || num < 1 || num > *count) {
        printf("Ungültige Auswahl!\n");
        while (getchar() != '\n');
        printf("\nWeiter mit Enter ...");
        getchar();
        return;
    }

    for (int i = num - 1; i < *count - 1; i++) {
        strcpy(items[i], items[i + 1]);
    }
    (*count)--;

    printf("\nArtikel gelöscht!\n");
    printf("\nWeiter mit Enter ...");
    getchar(); getchar();
}

// --- Hauptmenü: Liste bearbeiten ---
void edit_list(const char *filename) {
    char items[MAX_ITEMS][MAX_LEN];
    int count = load_list(filename, items);
    Database db;
    if (select_database(&db) != 0) {
        return;
    }
    int choice = 0;

    while (1) {
        //clear_screen();
        printf("=========== Einkaufsliste bearbeiten ===========\n");
        printf("[1] Liste anzeigen\n");
        printf("[2] Artikel hinzufügen\n");
        printf("[3] Artikel löschen\n");
        printf("[4] Speichern & zurück zum Hauptmenü\n");
        printf("===============================================\n");
        printf("Auswahl: ");

        if (scanf("%d", &choice) != 1) {
            printf("Ungültige Eingabe!\n");
            while (getchar() != '\n');
            continue;
        }

       // clear_screen();

        switch (choice) {
            case 1:
                print_list(items, count);
                printf("\nWeiter mit Enter ...");
                getchar(); getchar();
                break;

            case 2:
                add_item(&db, items, &count);
                break;

            case 3:
                remove_item(items, &count);
                break;

            case 4:
                save_list(filename, items, count);
                printf("Änderungen gespeichert. Zurück zum Hauptmenü...\n");
                printf("\nWeiter mit Enter ...");
                getchar(); getchar();
                return;

            default:
                printf("Ungültige Auswahl!\n");
                printf("\nWeiter mit Enter ...");
                getchar(); getchar();
                break;
        }
    }
}
