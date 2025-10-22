#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "druck_einkauf.h"

/**
 * Gibt den Inhalt einer gespeicherten Einkaufsliste aus.
 * Die Datei wird zeilenweise gelesen und mit Zeilennummern ausgegeben.
 */
void druck_einkauf(const char *filename) {
    FILE *file = fopen(filename, "r");

    if (file == NULL) {
        printf("Fehler: Die Datei '%s' konnte nicht geöffnet werden.\n", filename);
        printf("Bitte stelle sicher, dass sie existiert oder bereits angelegt wurde.\n");
        return;
    }

    printf("=========================================\n");
    printf("        Einkaufsliste: %s\n", filename);
    printf("=========================================\n");

    char line[256];
    int line_num = 0;
    int empty = 1;

    while (fgets(line, sizeof(line), file)) {
        empty = 0;
        // Entfernt das Zeilenende (\n), falls vorhanden
        line[strcspn(line, "\r\n")] = '\0';
        printf("%2d | %s\n", ++line_num, line);
    }

    if (empty) {
        printf("(Die Einkaufsliste ist leer.)\n");
    }

    printf("=========================================\n");

    char answer[8];
    for (;;) {
        printf("Soll die Liste als .txt gespeichert werden? (j/n): ");
        if (!fgets(answer, sizeof(answer), stdin)) {
            clearerr(stdin);
            continue;
        }
        answer[strcspn(answer, "\r\n")] = '\0';
        if (answer[0] == '\0') {
            continue;
        }
        if (answer[0] == 'j' || answer[0] == 'J') {
            FILE *out = NULL;
            char path[512];
            while (!out) {
                printf("Pfad für die Textdatei: ");
                if (!fgets(path, sizeof(path), stdin)) {
                    clearerr(stdin);
                    continue;
                }
                path[strcspn(path, "\r\n")] = '\0';
                if (path[0] == '\0') {
                    continue;
                }
                out = fopen(path, "w");
                if (!out) {
                    printf("Fehler: Die Datei '%s' konnte nicht gespeichert werden.\n", path);
                }
            }
            rewind(file);
            fprintf(out, "=========================================\n");
            fprintf(out, "        Einkaufsliste: %s\n", filename);
            fprintf(out, "=========================================\n");
            line_num = 0;
            empty = 1;
            while (fgets(line, sizeof(line), file)) {
                empty = 0;
                line[strcspn(line, "\r\n")] = '\0';
                fprintf(out, "%2d | %s\n", ++line_num, line);
            }
            if (empty) {
                fprintf(out, "(Die Einkaufsliste ist leer.)\n");
            }
            fprintf(out, "=========================================\n");
            fclose(out);
            printf("Einkaufsliste wurde unter '%s' gespeichert.\n", path);
            break;
        }
        if (answer[0] == 'n' || answer[0] == 'N') {
            break;
        }
    }

    fclose(file);
}
