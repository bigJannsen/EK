#ifndef CRUD_DATABASE_H
#define CRUD_DATABASE_H

#include <stddef.h>

#define DB_MAX_EINTRAEGE 512
#define DB_MAX_TEXTLAENGE 128
#define DB_MAX_DATEINAME 260

typedef struct {
    int id;
    char artikel[DB_MAX_TEXTLAENGE];
    char anbieter[DB_MAX_TEXTLAENGE];
    int preis_ct;
    char menge[DB_MAX_TEXTLAENGE];
} DatenbankEintrag;

typedef struct {
    DatenbankEintrag eintraege[DB_MAX_EINTRAEGE];
    int anzahl;
    char dateiname[DB_MAX_DATEINAME];
} Datenbank;

typedef DatenbankEintrag DatabaseEntry;
typedef Datenbank Database;

int lade_datenbank(const char *dateipfad, Datenbank *datenbank);
int speichere_datenbank(const Datenbank *datenbank);
void zeige_datenbank(const Datenbank *datenbank);
int bearbeite_datenbankeintrag(Datenbank *datenbank, int index);
void bearbeite_datenbank(Datenbank *datenbank);
int liste_csv_dateien(const char *verzeichnis, char dateien[][DB_MAX_DATEINAME], int max_dateien);

#define DB_MAX_ENTRIES DB_MAX_EINTRAEGE
#define DB_MAX_TEXT DB_MAX_TEXTLAENGE
#define DB_MAX_FILENAME DB_MAX_DATEINAME

#define load_database lade_datenbank
#define save_database speichere_datenbank
#define print_database zeige_datenbank
#define edit_database_entry bearbeite_datenbankeintrag
#define edit_database bearbeite_datenbank
#define list_csv_files liste_csv_dateien

#endif
