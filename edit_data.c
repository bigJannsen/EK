#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edit_data.h"
#include "crud_database.h"

#define MAX_FILES 128

static void wait_for_enter(void) {
    printf("\nWeiter mit Enter ...");
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
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

static const char *basename_of(const char *path) {
    const char *slash = strrchr(path, '/');
#ifdef _WIN32
    const char *backslash = strrchr(path, '\\');
    if (!slash || (backslash && backslash > slash)) slash = backslash;
#endif
    if (!slash) return path;
    return slash + 1;
}

static int handle_database(const char *filepath, Database *db) {
    if (!db) return -1;
    if (load_database(filepath, db) != 0) {
        printf("Datei konnte nicht geladen werden.\n");
        wait_for_enter();
        return -1;
    }
    return 0;
}

static int choose_database(const char *directory, Database *db) {
    char files[MAX_FILES][DB_MAX_FILENAME];
    int count = list_csv_files(directory, files, MAX_FILES);
    if (count < 0) {
        printf("Ordner konnte nicht gelesen werden.\n");
        wait_for_enter();
        return -1;
    }
    if (count == 0) {
        printf("Keine CSV-Dateien gefunden.\n");
        wait_for_enter();
        return -1;
    }
    printf("Verfügbare Datenbanken:\n");
    for (int i = 0; i < count; i++) {
        printf("[%d] %s\n", i + 1, basename_of(files[i]));
    }
    int choice = read_choice("Auswahl: ");
    if (choice < 1 || choice > count) {
        printf("Ungültige Auswahl.\n");
        wait_for_enter();
        return -1;
    }
    return handle_database(files[choice - 1], db);
}

void edit_data_menu(const char *directory) {
    if (!directory || directory[0] == '\0') {
        directory = DATA_DIRECTORY;
    }
    Database db;
    int db_loaded = 0;
    int dirty = 0;
    for (;;) {
        printf("\n===========================================\n");
        printf("Artikel und Preise editieren\n");
        printf("===========================================\n");
        printf("<0> Zum Hauptmenue\n");
        printf("<1> Datei laden (ehem. Datenbank aus ordner laden) \n");
        printf("<2> Datei sichern (ehem. Änderungen speichern) \n");
        printf("<3> Artikel/Preis loeschen (ehem. Unterpunkt von Eintrag bearbeiten) \n");
        printf("<4> Artikel/Preis hinzufuegen (ehem. Unterpunkt von Eintrag bearbeiten) \n");
        printf("<5> Artikel/Preis aendern (ehem. Unterpunkt von Eintrag bearbeiten) \n");
        printf("<6> Artikel/Preis auflisten (ehem. Einträge anzeigen) \n");
        int choice = read_choice("Ihre Auswahl: ");
        switch (choice) {
            case 0:
                if (db_loaded && dirty) {
                    printf("Es gibt ungespeicherte Änderungen. Trotzdem verlassen? (j/n): ");
                    char answer[8];
                    read_line(answer, sizeof answer);
                    if (!(answer[0] == 'j' || answer[0] == 'J')) {
                        break;
                    }
                }
                return;
            case 1:
                if (choose_database(directory, &db) == 0) {
                    db_loaded = 1;
                    dirty = 0;
                    printf("Datei geladen.\n");
                }
                break;
            case 2:
                if (!db_loaded) {
                    printf("Keine Datei geladen.\n");
                    break;
                }
                if (save_database(&db) == 0) {
                    dirty = 0;
                    printf("Gespeichert.\n");
                } else {
                    printf("Speichern fehlgeschlagen.\n");
                }
                break;
            case 3:
                if (!db_loaded) {
                    printf("Keine Datei geladen.\n");
                    break;
                }
                if (db.count == 0) {
                    printf("Keine Einträge vorhanden.\n");
                    break;
                }
                print_database(&db);
                {
                    int index = read_choice("Eintragsnummer (1-basig): ");
                    if (index < 1 || index > db.count) {
                        printf("Ungültige Auswahl.\n");
                        break;
                    }
                    for (int i = index; i < db.count; i++) {
                        db.entries[i - 1] = db.entries[i];
                    }
                    db.count--;
                    dirty = 1;
                    printf("Eintrag gelöscht.\n");
                }
                break;
            case 4:
                if (!db_loaded) {
                    printf("Keine Datei geladen.\n");
                    break;
                }
                if (db.count >= DB_MAX_ENTRIES) {
                    printf("Kein Platz für weitere Einträge.\n");
                    break;
                }
                {
                    char buffer[DB_MAX_TEXT];
                    DatabaseEntry *entry = &db.entries[db.count];
                    printf("ID: ");
                    read_line(buffer, sizeof buffer);
                    if (buffer[0] == '\0') {
                        printf("ID benötigt.\n");
                        break;
                    }
                    char *endptr = NULL;
                    long id = strtol(buffer, &endptr, 10);
                    if (!endptr || *endptr != '\0') {
                        printf("Ungültige ID.\n");
                        break;
                    }
                    entry->id = (int)id;
                    printf("Artikel: ");
                    read_line(entry->artikel, sizeof entry->artikel);
                    if (entry->artikel[0] == '\0') {
                        printf("Artikel benötigt.\n");
                        break;
                    }
                    printf("Anbieter: ");
                    read_line(entry->anbieter, sizeof entry->anbieter);
                    printf("Preis in ct: ");
                    read_line(buffer, sizeof buffer);
                    if (buffer[0] == '\0') {
                        printf("Preis benötigt.\n");
                        break;
                    }
                    endptr = NULL;
                    long preis = strtol(buffer, &endptr, 10);
                    if (!endptr || *endptr != '\0') {
                        printf("Ungültiger Preis.\n");
                        break;
                    }
                    entry->preis_ct = (int)preis;
                    printf("Menge: ");
                    read_line(entry->menge, sizeof entry->menge);
                    db.count++;
                    dirty = 1;
                    printf("Eintrag hinzugefügt.\n");
                }
                break;
            case 5:
                if (!db_loaded) {
                    printf("Keine Datei geladen.\n");
                    break;
                }
                if (db.count == 0) {
                    printf("Keine Einträge vorhanden.\n");
                    break;
                }
                print_database(&db);
                {
                    int index = read_choice("Eintragsnummer (1-basig): ");
                    if (index < 1 || index > db.count) {
                        printf("Ungültige Auswahl.\n");
                        break;
                    }
                    if (edit_database_entry(&db, index - 1) == 0) {
                        dirty = 1;
                    }
                }
                break;
            case 6:
                if (!db_loaded) {
                    printf("Keine Datei geladen.\n");
                    break;
                }
                print_database(&db);
                break;
            default:
                printf("Ungültige Auswahl.\n");
                break;
        }
    }
}
