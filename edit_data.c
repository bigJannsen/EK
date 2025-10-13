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

static void handle_database(const char *filepath) {
    Database db;
    if (load_database(filepath, &db) != 0) {
        printf("Datei konnte nicht geladen werden.\n");
        wait_for_enter();
        return;
    }
    edit_database(&db);
}

static void choose_database(const char *directory) {
    char files[MAX_FILES][DB_MAX_FILENAME];
    int count = list_csv_files(directory, files, MAX_FILES);
    if (count < 0) {
        printf("Ordner konnte nicht gelesen werden.\n");
        wait_for_enter();
        return;
    }
    if (count == 0) {
        printf("Keine CSV-Dateien gefunden.\n");
        wait_for_enter();
        return;
    }
    printf("Verfügbare Datenbanken:\n");
    for (int i = 0; i < count; i++) {
        printf("[%d] %s\n", i + 1, basename_of(files[i]));
    }
    int choice = read_choice("Auswahl: ");
    if (choice < 1 || choice > count) {
        printf("Ungültige Auswahl.\n");
        wait_for_enter();
        return;
    }
    handle_database(files[choice - 1]);
}

void edit_data_menu(const char *directory) {
    if (!directory || directory[0] == '\0') {
        directory = DATA_DIRECTORY;
    }
    for (;;) {
        printf("\n=========== Datenbankverwaltung ===========\n");
        printf("[1] Datenbank aus Ordner laden\n");
        printf("[2] Zurück\n");
        int choice = read_choice("Auswahl: ");
        switch (choice) {
            case 1:
                choose_database(directory);
                break;
            case 2:
                return;
            default:
                printf("Ungültige Auswahl.\n");
                break;
        }
    }
}
