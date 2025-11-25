#ifndef API_UTILS_H
#define API_UTILS_H

#include "config.h"
#include "database/database_core_defs.h"

#include <stddef.h>

#ifndef DATA_DIRECTORY
#define DATA_DIRECTORY "data"
#endif

#ifndef SHOPPING_LIST_PATH
#define SHOPPING_LIST_PATH DATA_DIRECTORY "/einkaufsliste.txt"
#endif

#ifndef SHOPPING_LIST_MAX_ITEMS
#define SHOPPING_LIST_MAX_ITEMS CONFIG_DEFAULT_MAX_ARTICLES
#endif

#ifndef SHOPPING_LIST_MAX_LEN
#define SHOPPING_LIST_MAX_LEN CONFIG_DEFAULT_MAX_STRING_LENGTH
#endif

typedef enum {
    UNIT_UNKNOWN,
    UNIT_GRAM,
    UNIT_MILLILITER,
    UNIT_PIECE
} UnitType;

typedef struct {
    double amount;
    UnitType type;
} Quantity;

int resolve_database_path(const char *name, char *out, size_t out_size);
int load_shopping_list(char items[][SHOPPING_LIST_MAX_LEN], int max_items);
int save_shopping_list(char items[][SHOPPING_LIST_MAX_LEN], int count);
void trim_whitespace_inplace(char *text);
void split_list_entry(const char *line, char *article, size_t article_size, char *provider, size_t provider_size);
void build_list_entry(const char *artikel, const char *anbieter, char *out, size_t out_size);
int entry_to_quantity(const DatabaseEntry *entry, Quantity *quantity);
const char *unit_label(UnitType type);
int parse_int_param(const char *value, int *out);
DatabaseEntry *find_entry_by_id(Database *db, int id);
int find_entry_index(const Database *db, int id);

#endif
