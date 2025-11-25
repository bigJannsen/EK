#include "database/csv_database_utils.h"

#include "database/quantity_unit_utils.h"
#include "database/text_input_utils.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include "windows/dirent.h"
#else
#include <dirent.h>
#endif
#include <sys/stat.h>

// CSV-Parsing und Dateimanagement für Datenbankdateien

int hat_csv_endung(const char *name) {
    size_t laenge = strlen(name);
    if (laenge < 4) {
        return 0;
    }
    int hat_punkt = tolower((unsigned char)name[laenge - 4]) == '.';
    int hat_c = tolower((unsigned char)name[laenge - 3]) == 'c';
    int hat_s = tolower((unsigned char)name[laenge - 2]) == 's';
    int hat_v = tolower((unsigned char)name[laenge - 1]) == 'v';
    if (hat_punkt != 0 && hat_c != 0 && hat_s != 0 && hat_v != 0) {
        return 1;
    }
    return 0;
}

int lade_datenbank(const char *dateipfad, Datenbank *datenbank) {
    if (dateipfad == NULL || datenbank == NULL) {
        return -1;
    }
    FILE *datei = fopen(dateipfad, "r");
    if (datei == NULL) {
        return -1;
    }
    datenbank->anzahl = 0;
    strncpy(datenbank->dateiname, dateipfad, DB_MAX_DATEINAME - 1);
    datenbank->dateiname[DB_MAX_DATEINAME - 1] = '\0';
    char zeile[512];
    while (fgets(zeile, sizeof zeile, datei) != NULL) {
        zeile[strcspn(zeile, "\r\n")] = '\0';
        if (zeile[0] == '\0') {
            continue;
        }
        char *felder[6];
        int feldanzahl = 0;
        char *teil = strtok(zeile, ",");
        while (teil != NULL && feldanzahl < 6) {
            felder[feldanzahl] = teil;
            feldanzahl++;
            teil = strtok(NULL, ",");
        }
        if (feldanzahl != 6 || datenbank->anzahl >= DB_MAX_EINTRAEGE) {
            continue;
        }
        for (int feld_index = 0; feld_index < 6; feld_index++) {
            entferne_leerraum(felder[feld_index]);
        }
        char *endstelle = NULL;
        long kennung = strtol(felder[0], &endstelle, 10);
        if (endstelle == NULL || *endstelle != '\0') {
            continue;
        }
        endstelle = NULL;
        long preis = strtol(felder[3], &endstelle, 10);
        if (endstelle == NULL || *endstelle != '\0') {
            continue;
        }
        double mengenwert = 0.0;
        if (lese_mengenwert_text(felder[4], &mengenwert) != 0) {
            continue;
        }
        if (mengenwert < 0.0) {
            continue;
        }
        DatenbankEintrag *eintrag = &datenbank->eintraege[datenbank->anzahl];
        datenbank->anzahl++;
        eintrag->id = (int)kennung;
        strncpy(eintrag->artikel, felder[1], DB_MAX_TEXTLAENGE - 1);
        eintrag->artikel[DB_MAX_TEXTLAENGE - 1] = '\0';
        strncpy(eintrag->anbieter, felder[2], DB_MAX_TEXTLAENGE - 1);
        eintrag->anbieter[DB_MAX_TEXTLAENGE - 1] = '\0';
        eintrag->preis_ct = (int)preis;
        eintrag->menge_wert = mengenwert;
        if (normalisiere_mengeneinheit(felder[5], eintrag->menge_einheit, sizeof eintrag->menge_einheit, &eintrag->menge_wert) != 0) {
            datenbank->anzahl--;
            continue;
        }
    }
    fclose(datei);
    return 0;
}

int speichere_datenbank(const Datenbank *datenbank) {
    if (datenbank == NULL) {
        return -1;
    }
    FILE *datei = fopen(datenbank->dateiname, "w");
    if (datei == NULL) {
        return -1;
    }
    for (int eintrags_index = 0; eintrags_index < datenbank->anzahl; eintrags_index++) {
        const DatenbankEintrag *eintrag = &datenbank->eintraege[eintrags_index];
        char mengen_text[32];
        formatiere_mengenwert(eintrag->menge_wert, mengen_text, sizeof mengen_text);
        fprintf(datei, "%d,%s,%s,%d,%s,%s\n", eintrag->id, eintrag->artikel, eintrag->anbieter, eintrag->preis_ct, mengen_text, eintrag->menge_einheit);
    }
    fclose(datei);
    return 0;
}

int liste_csv_dateien(const char *verzeichnis, char dateien[][DB_MAX_DATEINAME], int max_dateien) {
    if (verzeichnis == NULL || dateien == NULL || max_dateien <= 0) {
        return -1;
    }
    DIR *verzeichnis_stream = opendir(verzeichnis);
    if (verzeichnis_stream == NULL) {
        return -1;
    }
    struct dirent *eintrag;
    int anzahl = 0;
    while (anzahl < max_dateien) {
        eintrag = readdir(verzeichnis_stream);
        if (eintrag == NULL) {
            break;
        }
        if (strcmp(eintrag->d_name, ".") == 0 || strcmp(eintrag->d_name, "..") == 0) {
            continue;
        }
        if (hat_csv_endung(eintrag->d_name) == 0) {
            continue;
        }
        char pfad[DB_MAX_DATEINAME];
        snprintf(pfad, sizeof pfad, "%s/%s", verzeichnis, eintrag->d_name);
        struct stat status;
        if (stat(pfad, &status) == -1) {
            continue;
        }
        if (S_ISREG(status.st_mode) == 0) {
            continue;
        }
        strncpy(dateien[anzahl], pfad, DB_MAX_DATEINAME - 1);
        dateien[anzahl][DB_MAX_DATEINAME - 1] = '\0';
        anzahl++;
    }
    closedir(verzeichnis_stream);
    return anzahl;
}

