#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "edit_list.h"
#include "crud_database.h"
#include "edit_data.h"

#define MAX_LISTENEINTRAEGE 100
#define MAX_ZEICHEN 256
#define MAX_DATEIEN 128

// --- Hilfsfunktion: Bildschirm löschen ---
// Robin
//Löscht den Konsoleninhalt plattformabhängig
//Sorgt für eine aufgeräumte Darstellung zwischen Menüs
static void loesche_bildschirm() {
#ifdef _WIN32
    system("cls");
#else
    system("clear");
#endif
}

// --- Liest vorhandene Liste ein ---
// Robin
//Liest die vorhandene Einkaufsliste aus einer Datei in ein Array
//Entfernt Zeilenumbrüche und zählt die vorhandenen Einträge
static int lade_liste(const char *dateiname, char eintraege[MAX_LISTENEINTRAEGE][MAX_ZEICHEN]) {
    FILE *datei = fopen(dateiname, "r");
    int anzahl = 0;

    if (datei == NULL) {
        return 0; // Datei existiert noch nicht → leere Liste
    }

    while (fgets(eintraege[anzahl], MAX_ZEICHEN, datei) && anzahl < MAX_LISTENEINTRAEGE) {
        eintraege[anzahl][strcspn(eintraege[anzahl], "\r\n")] = '\0'; // Zeilenende entfernen
        anzahl++;
    }

    fclose(datei);
    return anzahl;
}

// --- Speichert Liste ---
// Robin
//Schreibt alle Listenelemente in die angegebene Datei
//Ersetzt den vorherigen Inhalt vollständig durch die aktuelle Liste
static void speichere_liste(const char *dateiname, char eintraege[MAX_LISTENEINTRAEGE][MAX_ZEICHEN], int anzahl) {
    FILE *datei = fopen(dateiname, "w");
    if (!datei) {
        perror("Fehler beim Speichern der Datei");
        return;
    }

    for (int i = 0; i < anzahl; i++) {
        fprintf(datei, "%s\n", eintraege[i]);
    }
    fclose(datei);
}

// --- Zeigt Liste an ---
// Robin
//Stellt die aktuellen Listeneinträge übersichtlich in der Konsole dar
//Bereitet Artikel und Anbieter getrennt auf falls verfügbar
static void zeige_liste(char eintraege[MAX_LISTENEINTRAEGE][MAX_ZEICHEN], int anzahl) {
    printf("=========================================\n");
    printf("          Aktuelle Einkaufsliste\n");
    printf("=========================================\n");
    if (anzahl == 0) {
        printf("(Die Liste ist leer.)\n");
    } else {
        for (int i = 0; i < anzahl; i++) {
            const char *trenner = strchr(eintraege[i], '|');
            if (trenner) {
                size_t artikel_laenge = (size_t)(trenner - eintraege[i]);
                char artikel[MAX_ZEICHEN];
                if (artikel_laenge >= sizeof artikel) artikel_laenge = sizeof artikel - 1;
                strncpy(artikel, eintraege[i], artikel_laenge);
                artikel[artikel_laenge] = '\0';
                const char *anbieter = trenner + 1;
                printf("%2d | %s (%s)\n", i + 1, artikel, anbieter);
            } else {
                printf("%2d | %s\n", i + 1, eintraege[i]);
            }
        }
    }
    printf("=========================================\n");
}

// --- Artikel hinzufügen ---
// Robin
//Leert verbleibende Zeichen im Eingabepuffer nach einer scanf Abfrage
//Verhindert dass Restzeichen nachfolgende Eingaben stören
static void leere_eingabe(void) {
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
    }
}

// Robin
//Ermittelt den Dateinamen ohne Pfadanteile plattformübergreifend
//Wird genutzt um Dateilisten benutzerfreundlich anzuzeigen
static const char *basisname_von(const char *pfad) {
    const char *schraegstrich = strrchr(pfad, '/');
#ifdef _WIN32
    const char *rueckwaertsschrägstrich = strrchr(pfad, '\\');
    if (!schraegstrich || (rueckwaertsschrägstrich && rueckwaertsschrägstrich > schraegstrich)) schraegstrich = rueckwaertsschrägstrich;
#endif
    if (!schraegstrich) return pfad;
    return schraegstrich + 1;
}

// Robin
//Zeigt verfügbare Datenbanken an und lädt die gewählte Datei
//Validiert die Auswahl und fordert bei Fehlern zur Wiederholung auf
static int waehle_datenbank(Datenbank *datenbank) {
    char dateien[MAX_DATEIEN][DB_MAX_DATEINAME];
    int anzahl = liste_csv_dateien(DATEN_VERZEICHNIS, dateien, MAX_DATEIEN);
    if (anzahl <= 0) {
        printf("Keine Datenbank gefunden.\n");
        printf("\nWeiter mit Enter ...");
        getchar();
        return -1;
    }
    printf("Verfügbare Datenbanken:\n");
    for (int i = 0; i < anzahl; i++) {
        printf("[%d] %s\n", i + 1, basisname_von(dateien[i]));
    }
    printf("Auswahl: ");
    int auswahl;
    if (scanf("%d", &auswahl) != 1 || auswahl < 1 || auswahl > anzahl) {
        printf("Ungültige Auswahl.\n");
        leere_eingabe();
        printf("\nWeiter mit Enter ...");
        getchar();
        return -1;
    }
    leere_eingabe();
    if (lade_datenbank(dateien[auswahl - 1], datenbank) != 0) {
        printf("Datei konnte nicht geladen werden.\n");
        printf("\nWeiter mit Enter ...");
        getchar();
        return -1;
    }
    return 0;
}

// Robin
//Durchsucht die geladene Datenbank nach einer ID und liefert den Eintrag
//Dient als Grundlage für das Hinzufügen von Artikeln zur Liste
static DatenbankEintrag *finde_eintrag(Datenbank *datenbank, int kennung) {
    if (!datenbank) return NULL;
    for (int i = 0; i < datenbank->anzahl; i++) {
        if (datenbank->eintraege[i].id == kennung) return &datenbank->eintraege[i];
    }
    return NULL;
}

// Robin
//Fügt der Liste einen Artikel aus der Datenbank hinzu
//Übernimmt Artikel und Anbieter in die lokale Einkaufsliste
static void fuege_artikel_hinzu(Datenbank *datenbank, char eintraege[MAX_LISTENEINTRAEGE][MAX_ZEICHEN], int *anzahl) {
    if (*anzahl >= MAX_LISTENEINTRAEGE) {
        printf("Die Liste ist voll! (max. %d Einträge)\n", MAX_LISTENEINTRAEGE);
        printf("\nWeiter mit Enter ...");
        getchar(); getchar();
        return;
    }
    if (!datenbank || datenbank->anzahl == 0) {
        printf("Keine Datenbank geladen.\n");
        printf("\nWeiter mit Enter ...");
        getchar(); getchar();
        return;
    }
    zeige_datenbank(datenbank);
    printf("ID des Artikels: ");
    int kennung;
    if (scanf("%d", &kennung) != 1) {
        printf("Ungültige Eingabe!\n");
        leere_eingabe();
        printf("\nWeiter mit Enter ...");
        getchar(); getchar();
        return;
    }
    leere_eingabe();
    DatenbankEintrag *eintrag = finde_eintrag(datenbank, kennung);
    if (!eintrag) {
        printf("ID nicht gefunden.\n");
        printf("\nWeiter mit Enter ...");
        getchar(); getchar();
        return;
    }
    snprintf(eintraege[*anzahl], MAX_ZEICHEN, "%s|%s", eintrag->artikel, eintrag->anbieter);
    (*anzahl)++;
    printf("\nArtikel hinzugefügt!\n");
    printf("\nWeiter mit Enter ...");
    getchar();
}

// --- Artikel löschen ---
// Robin
//Entfernt einen gewählten Artikel aus der Liste
//Verschiebt nachfolgende Einträge um die Lücke zu schließen
static void entferne_artikel(char eintraege[MAX_LISTENEINTRAEGE][MAX_ZEICHEN], int *anzahl) {
    if (*anzahl == 0) {
        printf("Die Liste ist leer, nichts zu löschen.\n");
        printf("\nWeiter mit Enter ...");
        getchar(); getchar();
        return;
    }

    zeige_liste(eintraege, *anzahl);

    printf("Nummer des zu löschenden Artikels: ");
    int nummer;
    if (scanf("%d", &nummer) != 1 || nummer < 1 || nummer > *anzahl) {
        printf("Ungültige Auswahl!\n");
        while (getchar() != '\n');
        printf("\nWeiter mit Enter ...");
        getchar();
        return;
    }

    for (int i = nummer - 1; i < *anzahl - 1; i++) {
        strcpy(eintraege[i], eintraege[i + 1]);
    }
    (*anzahl)--;

    printf("\nArtikel gelöscht!\n");
    printf("\nWeiter mit Enter ...");
    getchar(); getchar();
}

// --- Hauptmenü: Liste bearbeiten ---
// Robin
//Steuert das Menü zum Bearbeiten der Einkaufsliste
//Ermöglicht Anzeigen Hinzufügen Löschen und Speichern von Einträgen
void bearbeite_liste(const char *dateiname) {
    char eintraege[MAX_LISTENEINTRAEGE][MAX_ZEICHEN];
    int anzahl = lade_liste(dateiname, eintraege);
    Datenbank datenbank;
    if (waehle_datenbank(&datenbank) != 0) {
        return;
    }
    int auswahl = 0;

    while (1) {
        //clear_screen();
        printf("=========== Einkaufsliste bearbeiten ===========\n");
        printf("[1] Liste anzeigen\n");
        printf("[2] Artikel hinzufügen\n");
        printf("[3] Artikel löschen\n");
        printf("[4] Speichern & zurück zum Hauptmenü\n");
        printf("===============================================\n");
        printf("Auswahl: ");

        if (scanf("%d", &auswahl) != 1) {
            printf("Ungültige Eingabe!\n");
            while (getchar() != '\n');
            continue;
        }

       // clear_screen();

        switch (auswahl) {
            case 1:
                zeige_liste(eintraege, anzahl);
                printf("\nWeiter mit Enter ...");
                getchar(); getchar();
                break;

            case 2:
                fuege_artikel_hinzu(&datenbank, eintraege, &anzahl);
                break;

            case 3:
                entferne_artikel(eintraege, &anzahl);
                break;

            case 4:
                speichere_liste(dateiname, eintraege, anzahl);
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
