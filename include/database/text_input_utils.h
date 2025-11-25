#ifndef TEXT_INPUT_UTILS_H
#define TEXT_INPUT_UTILS_H

// Eingabe-, Sanitizing- und Validierungsfunktionen für Texte und Zahlen
#include <stddef.h>

void entferne_leerraum(char *text);
void lese_zeile(char *puffer, size_t groesse);

int pruefe_text_eingabe(const char *eingabe, size_t max_laenge);
int pruefe_optionalen_text(const char *eingabe, size_t max_laenge);
int pruefe_dateiname(const char *eingabe, size_t max_laenge);
int pruefe_ganzzahl_eingabe(const char *eingabe, long min_wert, long max_wert);
int pruefe_dezimalzahl_eingabe(const char *eingabe, double min_wert, double max_wert, int max_dezimalstellen);
int pruefe_flag_eingabe(const char *eingabe);

#endif // TEXT_INPUT_UTILS_H
