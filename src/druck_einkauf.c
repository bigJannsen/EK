#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "druck_einkauf.h"

// Robin
//Druckt eine bestehende Einkaufsliste im Terminal aus
//Liest die Datei zeilenweise und versieht jeden Eintrag mit einer Nummer
/**
 * Gibt den Inhalt einer gespeicherten Einkaufsliste aus.
 * Die Datei wird zeilenweise gelesen und mit Zeilennummern ausgegeben.
 */
void druck_einkauf(const char *dateiname) {
    FILE *datei = fopen(dateiname, "r");

    if (datei == NULL) {
        printf("Fehler: Die Datei '%s' konnte nicht geöffnet werden.\n", dateiname);
        printf("Bitte stelle sicher, dass sie existiert oder bereits angelegt wurde.\n");
        return;
    }

    printf("=========================================\n");
    printf("        Einkaufsliste: %s\n", dateiname);
    printf("=========================================\n");

    char zeile[256];
    int zeilennummer = 0;
    int leer = 1;

    while (fgets(zeile, sizeof(zeile), datei)) {
        leer = 0;
        // Entfernt das Zeilenende (\n), falls vorhanden
        zeile[strcspn(zeile, "\r\n")] = '\0';
        printf("%2d | %s\n", ++zeilennummer, zeile);
    }

    if (leer) {
        printf("(Die Einkaufsliste ist leer.)\n");
    }

    printf("=========================================\n");

    fclose(datei);
}
