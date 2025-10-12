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

    fclose(file);
}
