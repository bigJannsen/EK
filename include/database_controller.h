#ifndef DATABASE_CONTROLLER_H
#define DATABASE_CONTROLLER_H

// High-Level-Steuerung für Datenbankoperationen
#include "csv_database_utils.h"
#include "quantity_unit_utils.h"
#include "text_input_utils.h"

int load_database(const char *dateipfad, Datenbank *datenbank);
int save_database(const Datenbank *datenbank);
int list_csv_files(const char *verzeichnis, char dateien[][DB_MAX_DATEINAME], int max_dateien);

#endif // DATABASE_CONTROLLER_H
