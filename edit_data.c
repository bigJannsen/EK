#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "edit_data.h"
#include "crud_database.h"

#define MAX_DATEIEN 128

static void warte_auf_enter(void) {
    printf("\nWeiter mit Enter ...");
    int c;
    while ((c = getchar()) != '\n' && c != EOF) {
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

static const char *basisname_von(const char *pfad) {
    const char *schraegstrich = strrchr(pfad, '/');
#ifdef _WIN32
    const char *rueckwaertsschrägstrich = strrchr(pfad, '\\');
    if (!schraegstrich || (rueckwaertsschrägstrich && rueckwaertsschrägstrich > schraegstrich)) schraegstrich = rueckwaertsschrägstrich;
#endif
    if (!schraegstrich) return pfad;
    return schraegstrich + 1;
}

static int behandle_datenbankdatei(const char *dateipfad, Datenbank *datenbank) {
    if (!datenbank) return -1;
    if (lade_datenbank(dateipfad, datenbank) != 0) {
        printf("Datei konnte nicht geladen werden.\n");
        warte_auf_enter();
        return -1;
    }
    return 0;
}

static int waehle_datenbank(const char *verzeichnis, Datenbank *datenbank) {
    char dateien[MAX_DATEIEN][DB_MAX_DATEINAME];
    int anzahl = liste_csv_dateien(verzeichnis, dateien, MAX_DATEIEN);
    if (anzahl < 0) {
        printf("Ordner konnte nicht gelesen werden.\n");
        warte_auf_enter();
        return -1;
    }
    if (anzahl == 0) {
        printf("Keine CSV-Dateien gefunden.\n");
        warte_auf_enter();
        return -1;
    }
    printf("Verfügbare Datenbanken:\n");
    for (int i = 0; i < anzahl; i++) {
        printf("[%d] %s\n", i + 1, basisname_von(dateien[i]));
    }
    int auswahl = lese_auswahl("Auswahl: ");
    if (auswahl < 1 || auswahl > anzahl) {
        printf("Ungültige Auswahl.\n");
        warte_auf_enter();
        return -1;
    }
    return behandle_datenbankdatei(dateien[auswahl - 1], datenbank);
}

void bearbeite_daten_menue(const char *verzeichnis) {
    if (!verzeichnis || verzeichnis[0] == '\0') {
        verzeichnis = DATEN_VERZEICHNIS;
    }
    Datenbank datenbank;
    int datenbank_geladen = 0;
    int geaendert = 0;
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
        int auswahl = lese_auswahl("Ihre Auswahl: ");
        switch (auswahl) {
            case 0:
                if (datenbank_geladen && geaendert) {
                    printf("Es gibt ungespeicherte Änderungen. Trotzdem verlassen? (j/n): ");
                    char antwort[8];
                    lese_zeile(antwort, sizeof antwort);
                    if (!(antwort[0] == 'j' || antwort[0] == 'J')) {
                        break;
                    }
                }
                return;
            case 1:
                if (waehle_datenbank(verzeichnis, &datenbank) == 0) {
                    datenbank_geladen = 1;
                    geaendert = 0;
                    printf("Datei geladen.\n");
                }
                break;
            case 2:
                if (!datenbank_geladen) {
                    printf("Keine Datei geladen.\n");
                    break;
                }
                if (speichere_datenbank(&datenbank) == 0) {
                    geaendert = 0;
                    printf("Gespeichert.\n");
                } else {
                    printf("Speichern fehlgeschlagen.\n");
                }
                break;
            case 3:
                if (!datenbank_geladen) {
                    printf("Keine Datei geladen.\n");
                    break;
                }
                if (datenbank.anzahl == 0) {
                    printf("Keine Einträge vorhanden.\n");
                    break;
                }
                zeige_datenbank(&datenbank);
                {
                    int position = lese_auswahl("Eintragsnummer (1-basig): ");
                    if (position < 1 || position > datenbank.anzahl) {
                        printf("Ungültige Auswahl.\n");
                        break;
                    }
                    for (int i = position; i < datenbank.anzahl; i++) {
                        datenbank.eintraege[i - 1] = datenbank.eintraege[i];
                    }
                    datenbank.anzahl--;
                    geaendert = 1;
                    printf("Eintrag gelöscht.\n");
                }
                break;
            case 4:
                if (!datenbank_geladen) {
                    printf("Keine Datei geladen.\n");
                    break;
                }
                if (datenbank.anzahl >= DB_MAX_EINTRAEGE) {
                    printf("Kein Platz für weitere Einträge.\n");
                    break;
                }
                {
                    char puffer[DB_MAX_TEXTLAENGE];
                    DatenbankEintrag *eintrag = &datenbank.eintraege[datenbank.anzahl];
                    printf("ID: ");
                    lese_zeile(puffer, sizeof puffer);
                    if (puffer[0] == '\0') {
                        printf("ID benötigt.\n");
                        break;
                    }
                    char *endstelle = NULL;
                    long kennung = strtol(puffer, &endstelle, 10);
                    if (!endstelle || *endstelle != '\0') {
                        printf("Ungültige ID.\n");
                        break;
                    }
                    eintrag->id = (int)kennung;
                    printf("Artikel: ");
                    lese_zeile(eintrag->artikel, sizeof eintrag->artikel);
                    if (eintrag->artikel[0] == '\0') {
                        printf("Artikel benötigt.\n");
                        break;
                    }
                    printf("Anbieter: ");
                    lese_zeile(eintrag->anbieter, sizeof eintrag->anbieter);
                    printf("Preis in ct: ");
                    lese_zeile(puffer, sizeof puffer);
                    if (puffer[0] == '\0') {
                        printf("Preis benötigt.\n");
                        break;
                    }
                    endstelle = NULL;
                    long preis = strtol(puffer, &endstelle, 10);
                    if (!endstelle || *endstelle != '\0') {
                        printf("Ungültiger Preis.\n");
                        break;
                    }
                    eintrag->preis_ct = (int)preis;
                    printf("Menge: ");
                    lese_zeile(eintrag->menge, sizeof eintrag->menge);
                    datenbank.anzahl++;
                    geaendert = 1;
                    printf("Eintrag hinzugefügt.\n");
                }
                break;
            case 5:
                if (!datenbank_geladen) {
                    printf("Keine Datei geladen.\n");
                    break;
                }
                if (datenbank.anzahl == 0) {
                    printf("Keine Einträge vorhanden.\n");
                    break;
                }
                zeige_datenbank(&datenbank);
                {
                    int position = lese_auswahl("Eintragsnummer (1-basig): ");
                    if (position < 1 || position > datenbank.anzahl) {
                        printf("Ungültige Auswahl.\n");
                        break;
                    }
                    if (bearbeite_datenbankeintrag(&datenbank, position - 1) == 0) {
                        geaendert = 1;
                    }
                }
                break;
            case 6:
                if (!datenbank_geladen) {
                    printf("Keine Datei geladen.\n");
                    break;
                }
                zeige_datenbank(&datenbank);
                break;
            default:
                printf("Ungültige Auswahl.\n");
                break;
        }
    }
}
