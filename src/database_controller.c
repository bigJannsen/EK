#include "database_controller.h"

// Orchestriert CSV-Zugriff, Mengenlogik und Validierung für Datenbankoperationen

int load_database(const char *dateipfad, Datenbank *datenbank) {
    return lade_datenbank(dateipfad, datenbank);
}

int save_database(const Datenbank *datenbank) {
    return speichere_datenbank(datenbank);
}

int list_csv_files(const char *verzeichnis, char dateien[][DB_MAX_DATEINAME], int max_dateien) {
    return liste_csv_dateien(verzeichnis, dateien, max_dateien);
}

