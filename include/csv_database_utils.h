#ifndef CSV_DATABASE_UTILS_H
#define CSV_DATABASE_UTILS_H

// CSV-Parsing, Speicherung und Dateisuche für Datenbanken
#include "database_core_defs.h"

int lade_datenbank(const char *dateipfad, Datenbank *datenbank);
int speichere_datenbank(const Datenbank *datenbank);
int hat_csv_endung(const char *name);
int liste_csv_dateien(const char *verzeichnis, char dateien[][DB_MAX_DATEINAME], int max_dateien);

#endif // CSV_DATABASE_UTILS_H
