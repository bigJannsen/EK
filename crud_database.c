#include "crud_database.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <dirent.h>
#include <sys/stat.h>

// Philipp
//Entfernt führende und nachfolgende Leerzeichen in einem Textpuffer
//Bereitet Nutzereingaben für weitere Verarbeitung vor ohne den Inhalt zu verändern
static void entferne_leerraum(char *text) {
    if (text == NULL) {
        return;
    }
    char *anfang = text;
    while (*anfang != '\0' && isspace((unsigned char)*anfang)) {
        anfang++;
    }
    if (anfang != text) {
        memmove(text, anfang, strlen(anfang) + 1);
    }
    size_t laenge = strlen(text);
    while (laenge > 0 && isspace((unsigned char)text[laenge - 1])) {
        text[laenge - 1] = '\0';
        laenge--;
    }
}

// Philipp
//Liest eine Zeile von stdin in einen Puffer ein und entfernt Zeilenumbrüche
//Sichert gegen fehlerhafte Eingaben ab indem ein leerer String erzeugt wird
static void lese_zeile(char *puffer, size_t groesse) {
    if (puffer == NULL || groesse == 0) {
        return;
    }
    if (fgets(puffer, (int)groesse, stdin) == NULL) {
        puffer[0] = '\0';
        clearerr(stdin);
        return;
    }
    puffer[strcspn(puffer, "\r\n")] = '\0';
}

// Robin
//Formatiert einen Mengenwert als Text mit gekürzten Nachkommastellen
//Bereitet Mengenangaben für Ausgaben und Dateispeicherungen vor
void formatiere_mengenwert(double wert, char *ziel, size_t groesse) {
    if (ziel == NULL || groesse == 0) {
        return;
    }
    snprintf(ziel, groesse, "%.6f", wert);
    char *komma = strchr(ziel, '.');
    if (komma == NULL) {
        return;
    }
    char *ende = ziel + strlen(ziel) - 1;
    while (ende > komma && *ende == '0') {
        *ende = '\0';
        ende--;
    }
    if (ende == komma) {
        *ende = '\0';
    }
}

// Alen
//Prüft eine eingegebene Mengeneinheit und wandelt sie in ein Standardformat um
//Passt bei Bedarf auch den Zahlenwert an um konsistente Einheiten zu gewährleisten
int normalisiere_mengeneinheit(const char *eingabe, char *ausgabe, size_t groesse, double *wert) {
    if (eingabe == NULL || ausgabe == NULL || groesse == 0 || wert == NULL) {
        return -1;
    }
    char puffer[16];
    size_t zeichen_index = 0;
    for (const char *zeichen = eingabe; *zeichen != '\0' && zeichen_index + 1 < sizeof puffer; ++zeichen) {
        puffer[zeichen_index] = (char)tolower((unsigned char)*zeichen);
        zeichen_index++;
    }
    puffer[zeichen_index] = '\0';
    if (strcmp(puffer, "g") == 0 || strcmp(puffer, "gramm") == 0) {
        strncpy(ausgabe, "g", groesse - 1);
        ausgabe[groesse - 1] = '\0';
        return 0;
    }
    if (strcmp(puffer, "kg") == 0 || strcmp(puffer, "kilogramm") == 0) {
        strncpy(ausgabe, "kg", groesse - 1);
        ausgabe[groesse - 1] = '\0';
        return 0;
    }
    if (strcmp(puffer, "l") == 0 || strcmp(puffer, "liter") == 0) {
        strncpy(ausgabe, "l", groesse - 1);
        ausgabe[groesse - 1] = '\0';
        return 0;
    }
    if (strcmp(puffer, "ml") == 0) {
        *wert /= 1000.0;
        strncpy(ausgabe, "l", groesse - 1);
        ausgabe[groesse - 1] = '\0';
        return 0;
    }
    if (strcmp(puffer, "stk") == 0 || strcmp(puffer, "st") == 0 || strcmp(puffer, "stück") == 0 || strcmp(puffer, "stueck") == 0) {
        strncpy(ausgabe, "stk", groesse - 1);
        ausgabe[groesse - 1] = '\0';
        return 0;
    }
    return -1;
}

// Alen
//Konvertiert einen Text mit Mengenangabe in eine Gleitkommazahl
//Unterstützt Komma als Dezimaltrennzeichen und stellt fehlerhafte Eingaben fest
int lese_mengenwert_text(const char *text, double *wert) {
    if (text == NULL || wert == NULL) {
        return -1;
    }
    char puffer[DB_MAX_TEXTLAENGE];
    strncpy(puffer, text, sizeof puffer - 1);
    puffer[sizeof puffer - 1] = '\0';
    for (char *zeichen = puffer; *zeichen != '\0'; ++zeichen) {
        if (*zeichen == ',') {
            *zeichen = '.';
        }
    }
    char *ende = NULL;
    double ergebnis = strtod(puffer, &ende);
    if (ende == puffer || (ende != NULL && *ende != '\0')) {
        return -1;
    }
    *wert = ergebnis;
    return 0;
}

// Alen
//Lädt eine CSV Datei in die Datenbankstruktur und validiert jeden Eintrag
//Ignoriert fehlerhafte Zeilen und übernimmt gültige Datensätze in den Speicher
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

// Alen
//Schreibt alle Datensätze der geladenen Datenbank in die verknüpfte Datei
//Verwendet formatierte Mengenwerte um konsistente CSV Einträge zu erzeugen
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

// Robin
//Gibt den Inhalt der Datenbank tabellarisch auf der Konsole aus
//Zeigt formatierte Mengen und Einheiten zur besseren Lesbarkeit an
void zeige_datenbank(const Datenbank *datenbank) {
    if (datenbank == NULL) {
        return;
    }
    printf("=============================================\n");
    printf("ID | Artikel | Anbieter | Preis(ct) | Menge\n");
    printf("=============================================\n");
    if (datenbank->anzahl == 0) {
        printf("Keine Einträge vorhanden.\n");
    } else {
        for (int eintrags_index = 0; eintrags_index < datenbank->anzahl; eintrags_index++) {
            const DatenbankEintrag *eintrag = &datenbank->eintraege[eintrags_index];
            char mengen_text[32];
            formatiere_mengenwert(eintrag->menge_wert, mengen_text, sizeof mengen_text);
            const char *einheit = eintrag->menge_einheit;
            if (strcmp(einheit, "stk") == 0) {
                einheit = "Stk";
            } else if (strcmp(einheit, "l") == 0) {
                einheit = "L";
            } else if (strcmp(einheit, "kg") == 0) {
                einheit = "Kg";
            }
            printf("%d | %s | %s | %d | %s %s\n", eintrag->id, eintrag->artikel, eintrag->anbieter, eintrag->preis_ct, mengen_text, einheit);
        }
    }
    printf("=============================================\n");
}

// Philipp
//Interaktiv bearbeitet einen einzelnen Datensatz anhand von Nutzereingaben
//Erlaubt Änderungen an Kennung Preis Menge und Einheit mit Validierung
int bearbeite_datenbankeintrag(Datenbank *datenbank, int eintrags_index) {
    if (datenbank == NULL || eintrags_index < 0 || eintrags_index >= datenbank->anzahl) {
        return -1;
    }
    DatenbankEintrag *eintrag = &datenbank->eintraege[eintrags_index];
    char puffer[DB_MAX_TEXTLAENGE];
    printf("ID (%d): ", eintrag->id);
    lese_zeile(puffer, sizeof puffer);
    if (puffer[0] != '\0') {
        char *endstelle = NULL;
        long wert = strtol(puffer, &endstelle, 10);
        if (endstelle != NULL && *endstelle == '\0') {
            eintrag->id = (int)wert;
        }
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
        if (endstelle != NULL && *endstelle == '\0') {
            eintrag->preis_ct = (int)wert;
        }
    }
    char mengen_text[32];
    formatiere_mengenwert(eintrag->menge_wert, mengen_text, sizeof mengen_text);
    printf("Menge (%s): ", mengen_text);
    lese_zeile(puffer, sizeof puffer);
    if (puffer[0] != '\0') {
        double neuer_wert = 0.0;
        if (lese_mengenwert_text(puffer, &neuer_wert) == 0 && neuer_wert >= 0.0) {
            eintrag->menge_wert = neuer_wert;
        }
    }
    const char *aktuelle_einheit = eintrag->menge_einheit;
    if (strcmp(aktuelle_einheit, "stk") == 0) {
        aktuelle_einheit = "Stk";
    } else if (strcmp(aktuelle_einheit, "l") == 0) {
        aktuelle_einheit = "L";
    } else if (strcmp(aktuelle_einheit, "kg") == 0) {
        aktuelle_einheit = "Kg";
    }
    printf("Einheit (%s): ", aktuelle_einheit);
    lese_zeile(puffer, sizeof puffer);
    if (puffer[0] != '\0') {
        double wert_dummy = eintrag->menge_wert;
        if (normalisiere_mengeneinheit(puffer, eintrag->menge_einheit, sizeof eintrag->menge_einheit, &wert_dummy) == 0) {
            eintrag->menge_wert = wert_dummy;
        }
    }
    return 0;
}

// Philipp
//Fragt wiederholt eine numerische Auswahl vom Benutzer ab
//Sichert die Eingabe ab indem nur valide Ganzzahlen akzeptiert werden
static int lese_auswahl(const char *hinweis) {
    char puffer[32];
    int wiederhole_eingabe = 1;
    while (wiederhole_eingabe == 1) {
        printf("%s", hinweis);
        lese_zeile(puffer, sizeof puffer);
        if (puffer[0] == '\0') {
            continue;
        }
        char *endstelle = NULL;
        long wert = strtol(puffer, &endstelle, 10);
        if (endstelle != NULL && *endstelle == '\0') {
            return (int)wert;
        }
    }
    return 0;
}

// Alen
//Steuert das Konsolenmenü zur Verwaltung der Datenbank
//Erlaubt das Anzeigen Bearbeiten und Speichern von Datensätzen
void bearbeite_datenbank(Datenbank *datenbank) {
    if (datenbank == NULL) {
        return;
    }
    int geaenderte_daten_vorhanden = 0;
    int hauptschleife_aktiv = 1;
    while (hauptschleife_aktiv == 1) {
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
                    if (bearbeite_datenbankeintrag(datenbank, position - 1) == 0) {
                        geaenderte_daten_vorhanden = 1;
                    }
                }
                break;
            case 3:
                if (speichere_datenbank(datenbank) == 0) {
                    geaenderte_daten_vorhanden = 0;
                    printf("Gespeichert.\n");
                } else {
                    printf("Speichern fehlgeschlagen.\n");
                }
                break;
            case 4:
                if (geaenderte_daten_vorhanden != 0) {
                    printf("Es gibt ungespeicherte Änderungen. Trotzdem verlassen? (j/n): ");
                    char antwort[8];
                    lese_zeile(antwort, sizeof antwort);
                    if (antwort[0] == 'j' || antwort[0] == 'J') {
                        return;
                    }
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

// Robin
//Prüft ob ein Dateiname auf die Erweiterung csv endet
//Hilft bei der Filterung passender Dateien in Verzeichnissen
static int hat_csv_endung(const char *name) {
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

// Robin
//Durchsucht ein Verzeichnis nach CSV Dateien und speichert die Pfade
//Begrenzt die Anzahl der Ergebnisse auf die vorgegebene Maximalliste
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
