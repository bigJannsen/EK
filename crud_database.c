#include "crud_database.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

static void entferne_leerraum(char *text) {
    if (!text) return;
    char *anfang = text;
    while (*anfang && isspace((unsigned char)*anfang)) anfang++;
    if (anfang != text) memmove(text, anfang, strlen(anfang) + 1);
    size_t laenge = strlen(text);
    while (laenge > 0 && isspace((unsigned char)text[laenge - 1])) {
        text[laenge - 1] = '\0';
        laenge--;
    }
}

static void lese_zeile(char *puffer, size_t groesse) {
    if (!puffer || groesse == 0) return;
    if (!fgets(puffer, (int)groesse, stdin)) {
        puffer[0] = '\0';
        clearerr(stdin);
        return;
    }
    puffer[strcspn(puffer, "\r\n")] = '\0';
}

int lade_datenbank(const char *dateipfad, Datenbank *datenbank) {
    if (!dateipfad || !datenbank) return -1;
    FILE *datei = fopen(dateipfad, "r");
    if (!datei) return -1;
    datenbank->anzahl = 0;
    strncpy(datenbank->dateiname, dateipfad, DB_MAX_DATEINAME - 1);
    datenbank->dateiname[DB_MAX_DATEINAME - 1] = '\0';
    char zeile[512];
    while (fgets(zeile, sizeof zeile, datei)) {
        zeile[strcspn(zeile, "\r\n")] = '\0';
        if (zeile[0] == '\0') continue;
        char *felder[5];
        int feldanzahl = 0;
        char *teil = strtok(zeile, ",");
        while (teil && feldanzahl < 5) {
            felder[feldanzahl++] = teil;
            teil = strtok(NULL, ",");
        }
        if (feldanzahl != 5 || datenbank->anzahl >= DB_MAX_EINTRAEGE) continue;
        for (int i = 0; i < 5; i++) entferne_leerraum(felder[i]);
        char *endstelle = NULL;
        long kennung = strtol(felder[0], &endstelle, 10);
        if (!endstelle || *endstelle != '\0') continue;
        endstelle = NULL;
        long preis = strtol(felder[3], &endstelle, 10);
        if (!endstelle || *endstelle != '\0') continue;
        DatenbankEintrag *eintrag = &datenbank->eintraege[datenbank->anzahl++];
        eintrag->id = (int)kennung;
        strncpy(eintrag->artikel, felder[1], DB_MAX_TEXTLAENGE - 1);
        eintrag->artikel[DB_MAX_TEXTLAENGE - 1] = '\0';
        strncpy(eintrag->anbieter, felder[2], DB_MAX_TEXTLAENGE - 1);
        eintrag->anbieter[DB_MAX_TEXTLAENGE - 1] = '\0';
        eintrag->preis_ct = (int)preis;
        strncpy(eintrag->menge, felder[4], DB_MAX_TEXTLAENGE - 1);
        eintrag->menge[DB_MAX_TEXTLAENGE - 1] = '\0';
    }
    fclose(datei);
    return 0;
}

int speichere_datenbank(const Datenbank *datenbank) {
    if (!datenbank) return -1;
    FILE *datei = fopen(datenbank->dateiname, "w");
    if (!datei) return -1;
    for (int i = 0; i < datenbank->anzahl; i++) {
        const DatenbankEintrag *eintrag = &datenbank->eintraege[i];
        fprintf(datei, "%d,%s,%s,%d,%s\n", eintrag->id, eintrag->artikel, eintrag->anbieter, eintrag->preis_ct, eintrag->menge);
    }
    fclose(datei);
    return 0;
}

void zeige_datenbank(const Datenbank *datenbank) {
    if (!datenbank) return;
    printf("=============================================\n");
    printf("ID | Artikel | Anbieter | Preis(ct) | Menge\n");
    printf("=============================================\n");
    if (datenbank->anzahl == 0) {
        printf("Keine Einträge vorhanden.\n");
    } else {
        for (int i = 0; i < datenbank->anzahl; i++) {
            const DatenbankEintrag *eintrag = &datenbank->eintraege[i];
            printf("%d | %s | %s | %d | %s\n", eintrag->id, eintrag->artikel, eintrag->anbieter, eintrag->preis_ct, eintrag->menge);
        }
    }
    printf("=============================================\n");
}

int bearbeite_datenbankeintrag(Datenbank *datenbank, int index) {
    if (!datenbank || index < 0 || index >= datenbank->anzahl) return -1;
    DatenbankEintrag *eintrag = &datenbank->eintraege[index];
    char puffer[DB_MAX_TEXTLAENGE];
    printf("ID (%d): ", eintrag->id);
    lese_zeile(puffer, sizeof puffer);
    if (puffer[0] != '\0') {
        char *endstelle = NULL;
        long wert = strtol(puffer, &endstelle, 10);
        if (endstelle && *endstelle == '\0') eintrag->id = (int)wert;
    }
    printf("Artikel (%s): ", eintrag->artikel);
    lese_zeile(puffer, sizeof puffer);
    if (puffer[0] != '\0') {
        strncpy(eintrag->artikel, puffer, DB_MAX_TEXTLAENGE - 1);
        eintrag->artikel[DB_MAX_TEXTLAENGE - 1] = '\0';
    }
    printf("Anbieter (%s): ", eintrag->anbieter);
    lese_zeile(puffer, sizeof puffer);
    if (puffer[0] != '\0') {
        strncpy(eintrag->anbieter, puffer, DB_MAX_TEXTLAENGE - 1);
        eintrag->anbieter[DB_MAX_TEXTLAENGE - 1] = '\0';
    }
    printf("Preis in ct (%d): ", eintrag->preis_ct);
    lese_zeile(puffer, sizeof puffer);
    if (puffer[0] != '\0') {
        char *endstelle = NULL;
        long wert = strtol(puffer, &endstelle, 10);
        if (endstelle && *endstelle == '\0') eintrag->preis_ct = (int)wert;
    }
    printf("Menge (%s): ", eintrag->menge);
    lese_zeile(puffer, sizeof puffer);
    if (puffer[0] != '\0') {
        strncpy(eintrag->menge, puffer, DB_MAX_TEXTLAENGE - 1);
        eintrag->menge[DB_MAX_TEXTLAENGE - 1] = '\0';
    }
    return 0;
}

static int lese_auswahl(const char *hinweis) {
    char puffer[32];
    for (;;) {
        printf("%s", hinweis);
        lese_zeile(puffer, sizeof puffer);
        if (puffer[0] == '\0') continue;
        char *endstelle = NULL;
        long wert = strtol(puffer, &endstelle, 10);
        if (endstelle && *endstelle == '\0') return (int)wert;
    }
}

void bearbeite_datenbank(Datenbank *datenbank) {
    if (!datenbank) return;
    int geaendert = 0;
    for (;;) {
        printf("\n=== Datenbank: %s ===\n", datenbank->dateiname);
        printf("[1] Einträge anzeigen\n");
        printf("[2] Eintrag bearbeiten\n");
        printf("[3] Änderungen speichern\n");
        printf("[4] Zurück\n");
        int auswahl = lese_auswahl("Auswahl: ");
        switch (auswahl) {
            case 1:
                zeige_datenbank(datenbank);
                break;
            case 2:
                if (datenbank->anzahl == 0) {
                    printf("Keine Einträge vorhanden.\n");
                    break;
                }
                zeige_datenbank(datenbank);
                {
                    int position = lese_auswahl("Eintragsnummer (1-basig): ");
                    if (position < 1 || position > datenbank->anzahl) {
                        printf("Ungültige Auswahl.\n");
                        break;
                    }
                    if (bearbeite_datenbankeintrag(datenbank, position - 1) == 0) geaendert = 1;
                }
                break;
            case 3:
                if (speichere_datenbank(datenbank) == 0) {
                    geaendert = 0;
                    printf("Gespeichert.\n");
                } else {
                    printf("Speichern fehlgeschlagen.\n");
                }
                break;
            case 4:
                if (geaendert) {
                    printf("Es gibt ungespeicherte Änderungen. Trotzdem verlassen? (j/n): ");
                    char antwort[8];
                    lese_zeile(antwort, sizeof antwort);
                    if (antwort[0] == 'j' || antwort[0] == 'J') return;
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

static int hat_csv_endung(const char *name) {
    size_t laenge = strlen(name);
    if (laenge < 4) return 0;
    return (tolower((unsigned char)name[laenge - 4]) == '.' &&
            tolower((unsigned char)name[laenge - 3]) == 'c' &&
            tolower((unsigned char)name[laenge - 2]) == 's' &&
            tolower((unsigned char)name[laenge - 1]) == 'v');
}

int liste_csv_dateien(const char *verzeichnis, char dateien[][DB_MAX_DATEINAME], int max_dateien) {
    if (!verzeichnis || !dateien || max_dateien <= 0) return -1;
    DIR *verzeichnis_stream = opendir(verzeichnis);
    if (!verzeichnis_stream) return -1;
    struct dirent *eintrag;
    int anzahl = 0;
    while ((eintrag = readdir(verzeichnis_stream)) != NULL && anzahl < max_dateien) {
        if (strcmp(eintrag->d_name, ".") == 0 || strcmp(eintrag->d_name, "..") == 0) continue;
        if (!hat_csv_endung(eintrag->d_name)) continue;
        char pfad[DB_MAX_DATEINAME];
        snprintf(pfad, sizeof pfad, "%s/%s", verzeichnis, eintrag->d_name);
        struct stat status;
        if (stat(pfad, &status) == -1 || !S_ISREG(status.st_mode)) continue;
        strncpy(dateien[anzahl], pfad, DB_MAX_DATEINAME - 1);
        dateien[anzahl][DB_MAX_DATEINAME - 1] = '\0';
        anzahl++;
    }
    closedir(verzeichnis_stream);
    return anzahl;
}
