#ifndef DATABASE_CORE_DEFS_H
#define DATABASE_CORE_DEFS_H

// Zentrale Strukturen und Konstanten für die Datenbankverwaltung
#include <stddef.h>

#define DB_MAX_EINTRAEGE 512
#define DB_MAX_TEXTLAENGE 128
#define DB_MAX_DATEINAME 260

typedef struct {
    int id;
    char artikel[DB_MAX_TEXTLAENGE];
    char anbieter[DB_MAX_TEXTLAENGE];
    int preis_ct;
    double menge_wert;
    char menge_einheit[8];
} DatenbankEintrag;

typedef struct {
    DatenbankEintrag eintraege[DB_MAX_EINTRAEGE];
    int anzahl;
    char dateiname[DB_MAX_DATEINAME];
} Datenbank;

typedef DatenbankEintrag DatabaseEntry;
typedef Datenbank Database;

#define DB_MAX_ENTRIES DB_MAX_EINTRAEGE
#define DB_MAX_TEXT DB_MAX_TEXTLAENGE
#define DB_MAX_FILENAME DB_MAX_DATEINAME

#endif // DATABASE_CORE_DEFS_H
