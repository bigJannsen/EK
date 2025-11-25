#include "quantity_unit_utils.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Normalisierung und Formatierung von Mengen- und Einheitswerten

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

