// Diese Datei prüft Nutzereingaben auf Extremwerte, korrekte Dezimaltrennzeichen, unerlaubte Zeichen in Zahlenfeldern sowie problematische Sonderzeichen.
#include "userinput_pruefung.h"

#include <ctype.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int ist_druckbares_zeichen(unsigned char zeichen) {
    if (zeichen < 32 || zeichen == 127) {
        return 0;
    }
    switch (zeichen) {
        case '<':
        case '>':
        case '|':
        case '\\':
            return 0;
        default:
            return 1;
    }
}

// Prüft, ob ein Text innerhalb der erlaubten Länge bleibt und nur zulässige druckbare Zeichen enthält.
int pruefe_text_eingabe(const char *eingabe, size_t max_laenge) {
    if (eingabe == NULL) {
        return -1;
    }
    size_t laenge = strlen(eingabe);
    if (laenge == 0 || laenge > max_laenge) {
        return -1;
    }
    for (size_t i = 0; i < laenge; ++i) {
        unsigned char zeichen = (unsigned char)eingabe[i];
        if (!ist_druckbares_zeichen(zeichen)) {
            return -1;
        }
    }
    return 0;
}

// Prüft optionalen Text auf erlaubte Länge und Zeichen, erlaubt aber leere Eingaben.
int pruefe_optionalen_text(const char *eingabe, size_t max_laenge) {
    if (eingabe == NULL || *eingabe == '\0') {
        return 0;
    }
    return pruefe_text_eingabe(eingabe, max_laenge);
}

// Validiert Dateinamen auf zulässige Zeichen und verhindert Pfadausbrüche.
int pruefe_dateiname(const char *eingabe, size_t max_laenge) {
    if (pruefe_text_eingabe(eingabe, max_laenge) != 0) {
        return -1;
    }
    if (strstr(eingabe, "..") != NULL) {
        return -1;
    }
    for (const char *lauf = eingabe; *lauf; ++lauf) {
        if (*lauf == '/' || *lauf == '\\') {
            return -1;
        }
    }
    return 0;
}

// Prüft Ganzzahlen auf erlaubte Länge, korrekten Aufbau und Wertebereich.
int pruefe_ganzzahl_eingabe(const char *eingabe, long min_wert, long max_wert) {
    if (eingabe == NULL) {
        return -1;
    }
    size_t laenge = strlen(eingabe);
    if (laenge == 0 || laenge > 18) {
        return -1;
    }
    size_t index = 0;
    if (eingabe[0] == '+' || eingabe[0] == '-') {
        index = 1;
    }
    if (index == laenge) {
        return -1;
    }
    for (; index < laenge; ++index) {
        if (!isdigit((unsigned char)eingabe[index])) {
            return -1;
        }
    }
    char *ende = NULL;
    long wert = strtol(eingabe, &ende, 10);
    if (ende == NULL || *ende != '\0') {
        return -1;
    }
    if (wert < min_wert || wert > max_wert) {
        return -1;
    }
    return 0;
}

// Prüft Dezimalzahlen auf korrektes Format, Dezimaltrennzeichen, maximale Stellenzahl und Wertebereich.
int pruefe_dezimalzahl_eingabe(const char *eingabe, double min_wert, double max_wert, int max_dezimalstellen) {
    if (eingabe == NULL || max_dezimalstellen < 0) {
        return -1;
    }
    size_t laenge = strlen(eingabe);
    if (laenge == 0 || laenge > 24) {
        return -1;
    }
    size_t index = 0;
    if (eingabe[0] == '+' || eingabe[0] == '-') {
        index = 1;
    }
    if (index == laenge) {
        return -1;
    }
    int trennzeichen_gefunden = 0;
    int stellen_nach_trenner = 0;
    for (; index < laenge; ++index) {
        unsigned char zeichen = (unsigned char)eingabe[index];
        if (zeichen == '.' || zeichen == ',') {
            if (trennzeichen_gefunden) {
                return -1;
            }
            trennzeichen_gefunden = 1;
            stellen_nach_trenner = 0;
            continue;
        }
        if (!isdigit(zeichen)) {
            return -1;
        }
        if (trennzeichen_gefunden) {
            stellen_nach_trenner++;
            if (stellen_nach_trenner > max_dezimalstellen) {
                return -1;
            }
        }
    }
    char kopie[32];
    strncpy(kopie, eingabe, sizeof kopie - 1);
    kopie[sizeof kopie - 1] = '\0';
    for (char *lauf = kopie; *lauf; ++lauf) {
        if (*lauf == ',') {
            *lauf = '.';
        }
    }
    char *ende = NULL;
    double wert = strtod(kopie, &ende);
    if (ende == NULL || *ende != '\0') {
        return -1;
    }
    if (!isfinite(wert)) {
        return -1;
    }
    if (wert < min_wert || wert > max_wert) {
        return -1;
    }
    return 0;
}

// Prüft, ob eine optionale Eingabe ausschließlich den Flagwert 0 oder 1 enthält.
int pruefe_flag_eingabe(const char *eingabe) {
    if (eingabe == NULL || *eingabe == '\0') {
        return 0;
    }
    if (strcmp(eingabe, "0") == 0 || strcmp(eingabe, "1") == 0) {
        return 0;
    }
    return -1;
}
