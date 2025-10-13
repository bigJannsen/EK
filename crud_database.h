#ifndef CRUD_DATABASE_H
#define CRUD_DATABASE_H

#include <stddef.h>

#define DB_MAX_ENTRIES 512
#define DB_MAX_TEXT 128
#define DB_MAX_FILENAME 260

typedef struct {
    int id;
    char artikel[DB_MAX_TEXT];
    char anbieter[DB_MAX_TEXT];
    int preis_ct;
    char menge[DB_MAX_TEXT];
} DatabaseEntry;

typedef struct {
    DatabaseEntry entries[DB_MAX_ENTRIES];
    int count;
    char filename[DB_MAX_FILENAME];
} Database;

int load_database(const char *filepath, Database *db);
int save_database(const Database *db);
void print_database(const Database *db);
int edit_database_entry(Database *db, int index);
void edit_database(Database *db);
int list_csv_files(const char *directory, char files[][DB_MAX_FILENAME], int max_files);

#endif
