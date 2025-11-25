#include "text_input_utils.h"

#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Sammlung von Sanitizing- und Validierungsfunktionen für Texteingaben

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

void entferne_leerraum(char *text) {
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

void lese_zeile(char *puffer, size_t groesse) {
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

int pruefe_optionalen_text(const char *eingabe, size_t max_laenge) {
    if (eingabe == NULL || *eingabe == '\0') {
        return 0;
    }
    return pruefe_text_eingabe(eingabe, max_laenge);
}

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

int pruefe_flag_eingabe(const char *eingabe) {
    if (eingabe == NULL || *eingabe == '\0') {
        return 0;
    }
    if (strcmp(eingabe, "0") == 0 || strcmp(eingabe, "1") == 0) {
        return 0;
    }
    return -1;
}

