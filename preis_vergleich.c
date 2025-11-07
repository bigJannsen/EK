#include "preis_vergleich.h"

#include "crud_database.h"
#include "edit_data.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_DATEIEN 128
#define LISTE_MAX_EINTRAEGE 100
#define LISTE_MAX_ZEICHEN 256
#define EINKAUFSLISTE_DATEI "einkaufsliste.txt"

typedef enum {
    EINHEIT_UNBEKANNT,
    EINHEIT_GRAMM,
    EINHEIT_MILLILITER,
    EINHEIT_STUECK
} Einheitstyp;

typedef struct {
    double menge;
    Einheitstyp typ;
} Mengenangabe;

static void lese_zeile(char *puffer, size_t groesse) {
    if (!puffer || groesse == 0) return;
    if (!fgets(puffer, (int)groesse, stdin)) {
        puffer[0] = '\0';
        clearerr(stdin);
        return;
    }
    puffer[strcspn(puffer, "\r\n")] = '\0';
}

static int lese_ganzzahl(const char *hinweis) {
    char puffer[64];
    for (;;) {
        printf("%s", hinweis);
        lese_zeile(puffer, sizeof puffer);
        if (puffer[0] == '\0') continue;
        char *ende = NULL;
        long wert = strtol(puffer, &ende, 10);
        if (ende && *ende == '\0') return (int)wert;
        printf("Ungültige Eingabe.\n");
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

static const char *einheits_bezeichnung(Einheitstyp typ) {
    switch (typ) {
        case EINHEIT_GRAMM: return "g";
        case EINHEIT_MILLILITER: return "ml";
        case EINHEIT_STUECK: return "Stück";
        default: return "";
    }
}

static int normalisiere_einheit(const char *einheit, Einheitstyp *typ, double *faktor) {
    if (!einheit || !typ || !faktor) return -1;
    if (strcmp(einheit, "g") == 0) {
        *typ = EINHEIT_GRAMM;
        *faktor = 1.0;
        return 0;
    }
    if (strcmp(einheit, "kg") == 0) {
        *typ = EINHEIT_GRAMM;
        *faktor = 1000.0;
        return 0;
    }
    if (strcmp(einheit, "l") == 0) {
        *typ = EINHEIT_MILLILITER;
        *faktor = 1000.0;
        return 0;
    }
    if (strcmp(einheit, "ml") == 0) {
        *typ = EINHEIT_MILLILITER;
        *faktor = 1.0;
        return 0;
    }
    if (strcmp(einheit, "stk") == 0 || strcmp(einheit, "st") == 0 || strcmp(einheit, "stück") == 0) {
        *typ = EINHEIT_STUECK;
        *faktor = 1.0;
        return 0;
    }
    return -1;
}

static int ermittle_mengenangabe(const DatenbankEintrag *eintrag, Mengenangabe *mengenangabe) {
    if (!eintrag || !mengenangabe) return -1;
    if (eintrag->menge_wert <= 0.0) return -1;
    Einheitstyp typ = EINHEIT_UNBEKANNT;
    double faktor = 0.0;
    if (normalisiere_einheit(eintrag->menge_einheit, &typ, &faktor) != 0) return -1;
    mengenangabe->menge = eintrag->menge_wert * faktor;
    mengenangabe->typ = typ;
    return 0;
}

static const char *anzeige_einheit(const char *einheit) {
    if (!einheit) return "";
    if (strcmp(einheit, "stk") == 0) return "Stk";
    if (strcmp(einheit, "l") == 0) return "L";
    if (strcmp(einheit, "kg") == 0) return "Kg";
    return einheit;
}

static void beschreibe_menge(const DatenbankEintrag *eintrag, char *ziel, size_t groesse) {
    if (!ziel || groesse == 0 || !eintrag) return;
    char wert[32];
    formatiere_mengenwert(eintrag->menge_wert, wert, sizeof wert);
    snprintf(ziel, groesse, "%s %s", wert, anzeige_einheit(eintrag->menge_einheit));
}

static int waehle_datenbank_pfad(const char *verzeichnis, char *ausgabepfad, size_t ausgabe_groesse) {
    char dateien[MAX_DATEIEN][DB_MAX_DATEINAME];
    int anzahl = liste_csv_dateien(verzeichnis, dateien, MAX_DATEIEN);
    if (anzahl <= 0) {
        printf("Keine Datenbanken gefunden.\n");
        return -1;
    }
    printf("Verfügbare Datenbanken:\n");
    for (int i = 0; i < anzahl; i++) {
        printf("[%d] %s\n", i + 1, basisname_von(dateien[i]));
    }
    for (;;) {
        int auswahl = lese_ganzzahl("Auswahl: ");
        if (auswahl >= 1 && auswahl <= anzahl) {
            strncpy(ausgabepfad, dateien[auswahl - 1], ausgabe_groesse - 1);
            ausgabepfad[ausgabe_groesse - 1] = '\0';
            return 0;
        }
        printf("Ungültige Auswahl.\n");
    }
}

static DatenbankEintrag *finde_eintrag_nach_id(Datenbank *datenbank, int kennung) {
    if (!datenbank) return NULL;
    for (int i = 0; i < datenbank->anzahl; i++) {
        if (datenbank->eintraege[i].id == kennung) return &datenbank->eintraege[i];
    }
    return NULL;
}

static DatenbankEintrag *waehle_eintrag(Datenbank *datenbank, const char *hinweis, int ausgeschlossene_id) {
    for (;;) {
        int kennung = lese_ganzzahl(hinweis);
        if (ausgeschlossene_id != -1 && kennung == ausgeschlossene_id) {
            printf("Dieser Eintrag wurde bereits gewählt.\n");
            continue;
        }
        DatenbankEintrag *eintrag = finde_eintrag_nach_id(datenbank, kennung);
        if (eintrag) return eintrag;
        printf("ID nicht gefunden.\n");
    }
}

static double lese_menge(Einheitstyp typ) {
    char puffer[64];
    const char *einheit = einheits_bezeichnung(typ);
    for (;;) {
        printf("Gewünschte Menge in %s: ", einheit);
        lese_zeile(puffer, sizeof puffer);
        if (puffer[0] == '\0') continue;
        for (char *zeichen = puffer; *zeichen; ++zeichen) {
            if (*zeichen == ',') *zeichen = '.';
        }
        char *ende = NULL;
        double wert = strtod(puffer, &ende);
        if (ende && *ende == '\0' && wert > 0.0) {
            return wert;
        }
        printf("Ungültige Eingabe.\n");
    }
}

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

static void teile_listeneintrag(const char *zeile, char *artikel, size_t artikel_groesse, char *anbieter, size_t anbieter_groesse) {
    if (!zeile) {
        if (artikel && artikel_groesse > 0) artikel[0] = '\0';
        if (anbieter && anbieter_groesse > 0) anbieter[0] = '\0';
        return;
    }
    const char *trenner = strchr(zeile, '|');
    if (trenner) {
        size_t laenge = (size_t)(trenner - zeile);
        if (artikel && artikel_groesse > 0) {
            if (laenge >= artikel_groesse) laenge = artikel_groesse - 1;
            strncpy(artikel, zeile, laenge);
            artikel[laenge] = '\0';
        }
        if (anbieter && anbieter_groesse > 0) {
            strncpy(anbieter, trenner + 1, anbieter_groesse - 1);
            anbieter[anbieter_groesse - 1] = '\0';
        }
    } else {
        if (artikel && artikel_groesse > 0) {
            strncpy(artikel, zeile, artikel_groesse - 1);
            artikel[artikel_groesse - 1] = '\0';
        }
        if (anbieter && anbieter_groesse > 0) anbieter[0] = '\0';
    }
    if (artikel) entferne_leerraum(artikel);
    if (anbieter) entferne_leerraum(anbieter);
}

static int lade_einkaufsliste(const char *dateiname, char eintraege[LISTE_MAX_EINTRAEGE][LISTE_MAX_ZEICHEN]) {
    FILE *datei = fopen(dateiname, "r");
    if (!datei) return 0;
    int anzahl = 0;
    while (anzahl < LISTE_MAX_EINTRAEGE && fgets(eintraege[anzahl], LISTE_MAX_ZEICHEN, datei)) {
        eintraege[anzahl][strcspn(eintraege[anzahl], "\r\n")] = '\0';
        anzahl++;
    }
    fclose(datei);
    return anzahl;
}

static void speichere_einkaufsliste(const char *dateiname, char eintraege[LISTE_MAX_EINTRAEGE][LISTE_MAX_ZEICHEN], int anzahl) {
    FILE *datei = fopen(dateiname, "w");
    if (!datei) {
        printf("Einkaufsliste konnte nicht gespeichert werden.\n");
        return;
    }
    for (int i = 0; i < anzahl; i++) {
        fprintf(datei, "%s\n", eintraege[i]);
    }
    fclose(datei);
}

static void vergleiche_einzeln(Datenbank *datenbank) {
    if (!datenbank) return;
    if (datenbank->anzahl < 2) {
        printf("Zu wenige Einträge für einen Vergleich.\n");
        return;
    }
    zeige_datenbank(datenbank);
    DatenbankEintrag *erster = waehle_eintrag(datenbank, "ID des ersten Eintrags: ", -1);
    DatenbankEintrag *zweiter = waehle_eintrag(datenbank, "ID des zweiten Eintrags: ", erster->id);
    Mengenangabe menge_erster;
    Mengenangabe menge_zweiter;
    if (ermittle_mengenangabe(erster, &menge_erster) != 0 ||
        ermittle_mengenangabe(zweiter, &menge_zweiter) != 0) {
        printf("Mengenangaben konnten nicht interpretiert werden.\n");
        return;
    }
    if (menge_erster.typ != menge_zweiter.typ) {
        printf("Die Einheiten der ausgewählten Produkte stimmen nicht überein.\n");
        return;
    }
    double gewuenschte_menge = lese_menge(menge_erster.typ);
    double preis_pro_einheit_erster = (double)erster->preis_ct / menge_erster.menge;
    double preis_pro_einheit_zweiter = (double)zweiter->preis_ct / menge_zweiter.menge;
    double gesamt_erster = preis_pro_einheit_erster * gewuenschte_menge;
    double gesamt_zweiter = preis_pro_einheit_zweiter * gewuenschte_menge;
    const char *einheit = einheits_bezeichnung(menge_erster.typ);
    printf("\nVergleich der Produkte für %.2f %s:\n", gewuenschte_menge, einheit);
    char beschreibung[64];
    printf("1) %s (%s)\n", erster->artikel, erster->anbieter);
    beschreibe_menge(erster, beschreibung, sizeof beschreibung);
    printf("   Menge pro Packung: %s\n", beschreibung);
    printf("   Preis pro Packung: %d ct (%.2f €)\n", erster->preis_ct, erster->preis_ct / 100.0);
    printf("   Preis pro %s: %.4f ct\n", einheit, preis_pro_einheit_erster);
    printf("   Preis für %.2f %s: %.2f € (%.2f ct)\n\n", gewuenschte_menge, einheit, gesamt_erster / 100.0, gesamt_erster);
    printf("2) %s (%s)\n", zweiter->artikel, zweiter->anbieter);
    beschreibe_menge(zweiter, beschreibung, sizeof beschreibung);
    printf("   Menge pro Packung: %s\n", beschreibung);
    printf("   Preis pro Packung: %d ct (%.2f €)\n", zweiter->preis_ct, zweiter->preis_ct / 100.0);
    printf("   Preis pro %s: %.4f ct\n", einheit, preis_pro_einheit_zweiter);
    printf("   Preis für %.2f %s: %.2f € (%.2f ct)\n", gewuenschte_menge, einheit, gesamt_zweiter / 100.0, gesamt_zweiter);
    const double epsilon = 1e-6;
    if (preis_pro_einheit_erster + epsilon < preis_pro_einheit_zweiter) {
        printf("\nGünstiger pro %s ist: %s (%s)\n", einheit, erster->artikel, erster->anbieter);
    } else if (preis_pro_einheit_zweiter + epsilon < preis_pro_einheit_erster) {
        printf("\nGünstiger pro %s ist: %s (%s)\n", einheit, zweiter->artikel, zweiter->anbieter);
    } else {
        printf("\nBeide Produkte sind pro %s gleich teuer.\n", einheit);
    }
}

static void vergleiche_einkaufsliste(Datenbank *datenbank, const char *listen_datei) {
    if (!datenbank || !listen_datei) return;
    char eintraege[LISTE_MAX_EINTRAEGE][LISTE_MAX_ZEICHEN];
    int anzahl = lade_einkaufsliste(listen_datei, eintraege);
    if (anzahl == 0) {
        printf("Die Einkaufsliste ist leer.\n");
        return;
    }
    int beste_indizes[LISTE_MAX_EINTRAEGE];
    for (int i = 0; i < anzahl; i++) beste_indizes[i] = -1;
    printf("\nPreisvergleich für die Einkaufsliste:\n");
    for (int i = 0; i < anzahl; i++) {
        char artikel[DB_MAX_TEXTLAENGE];
        char anbieter[DB_MAX_TEXTLAENGE];
        teile_listeneintrag(eintraege[i], artikel, sizeof artikel, anbieter, sizeof anbieter);
        if (artikel[0] == '\0') {
            printf("- Position %d konnte nicht gelesen werden.\n", i + 1);
            continue;
        }
        printf("\n%s:\n", artikel);
        int treffer = 0;
        int bester_index = -1;
        int bester_hat_menge = 0;
        double bester_einheitspreis = 0.0;
        int bester_packungspreis = 0;
        for (int j = 0; j < datenbank->anzahl; j++) {
            DatenbankEintrag *eintrag = &datenbank->eintraege[j];
            if (strcmp(eintrag->artikel, artikel) != 0) continue;
            treffer++;
            Mengenangabe mengenangabe;
            int hat_menge = 0;
            double einheitspreis = 0.0;
            if (ermittle_mengenangabe(eintrag, &mengenangabe) == 0 && mengenangabe.menge > 0.0) {
                hat_menge = 1;
                einheitspreis = (double)eintrag->preis_ct / mengenangabe.menge;
            }
            char beschreibung[64];
            beschreibe_menge(eintrag, beschreibung, sizeof beschreibung);
            printf("  - %s: %d ct (%.2f €) pro Packung, %s", eintrag->anbieter, eintrag->preis_ct, eintrag->preis_ct / 100.0, beschreibung);
            if (hat_menge) {
                printf(", %.4f ct pro %s", einheitspreis, einheits_bezeichnung(mengenangabe.typ));
            }
            if (anbieter[0] != '\0' && strcmp(anbieter, eintrag->anbieter) == 0) {
                printf(" [aktuelle Auswahl]");
            }
            printf("\n");
            if (hat_menge) {
                if (!bester_hat_menge || einheitspreis + 1e-6 < bester_einheitspreis) {
                    bester_index = j;
                    bester_hat_menge = 1;
                    bester_einheitspreis = einheitspreis;
                    bester_packungspreis = eintrag->preis_ct;
                }
            } else {
                if (bester_index == -1 || (!bester_hat_menge && eintrag->preis_ct < bester_packungspreis)) {
                    bester_index = j;
                    bester_packungspreis = eintrag->preis_ct;
                }
            }
        }
        if (treffer == 0) {
            printf("  Kein Anbieter gefunden.\n");
            continue;
        }
        if (bester_index >= 0) {
            DatenbankEintrag *bester = &datenbank->eintraege[bester_index];
            char beschreibung[64];
            beschreibe_menge(bester, beschreibung, sizeof beschreibung);
            printf("  -> Günstigster Anbieter: %s (%s) mit %d ct pro Packung\n", bester->anbieter, beschreibung, bester->preis_ct);
            beste_indizes[i] = bester_index;
        }
    }
    printf("\nErgebnisse in die Einkaufsliste übernehmen? (j/n): ");
    char antwort[8];
    lese_zeile(antwort, sizeof antwort);
    if (antwort[0] == 'j' || antwort[0] == 'J') {
        for (int i = 0; i < anzahl; i++) {
            if (beste_indizes[i] >= 0) {
                DatenbankEintrag *bester = &datenbank->eintraege[beste_indizes[i]];
                snprintf(eintraege[i], LISTE_MAX_ZEICHEN, "%s|%s", bester->artikel, bester->anbieter);
            }
        }
        speichere_einkaufsliste(listen_datei, eintraege, anzahl);
        printf("Einkaufsliste aktualisiert.\n");
    }
}

void preisvergleich_menue(const char *verzeichnis) {
    if (!verzeichnis || verzeichnis[0] == '\0') verzeichnis = DATEN_VERZEICHNIS;
    printf("[1] Einzelvergleich\n");
    printf("[2] Einkaufszettel vergleichen\n");
    int modus = lese_ganzzahl("Auswahl: ");
    if (modus != 1 && modus != 2) {
        printf("Ungültige Auswahl.\n");
        return;
    }
    char pfad[DB_MAX_DATEINAME];
    if (waehle_datenbank_pfad(verzeichnis, pfad, sizeof pfad) != 0) return;
    Datenbank datenbank;
    if (lade_datenbank(pfad, &datenbank) != 0) {
        printf("Datei konnte nicht geladen werden.\n");
        return;
    }
    if (modus == 1) {
        vergleiche_einzeln(&datenbank);
    } else {
        vergleiche_einkaufsliste(&datenbank, EINKAUFSLISTE_DATEI);
    }
}
