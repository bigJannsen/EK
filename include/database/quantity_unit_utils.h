#ifndef QUANTITY_UNIT_UTILS_H
#define QUANTITY_UNIT_UTILS_H

// Funktionen zum Normalisieren, Konvertieren und Formatieren von Mengenangaben
#include <stddef.h>
#include "database/database_core_defs.h"

void formatiere_mengenwert(double wert, char *ziel, size_t groesse);
int lese_mengenwert_text(const char *text, double *wert);
int normalisiere_mengeneinheit(const char *eingabe, char *ausgabe, size_t groesse, double *wert);

#endif // QUANTITY_UNIT_UTILS_H
