#ifndef USERINPUT_PRUEFUNG_H
#define USERINPUT_PRUEFUNG_H

#include <stddef.h>

int pruefe_text_eingabe(const char *eingabe, size_t max_laenge);
int pruefe_optionalen_text(const char *eingabe, size_t max_laenge);
int pruefe_dateiname(const char *eingabe, size_t max_laenge);
int pruefe_ganzzahl_eingabe(const char *eingabe, long min_wert, long max_wert);
int pruefe_dezimalzahl_eingabe(const char *eingabe, double min_wert, double max_wert, int max_dezimalstellen);
int pruefe_flag_eingabe(const char *eingabe);

#endif
