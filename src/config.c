#include "config.h"

#include <stdio.h>

// Globale Konfigurationsinstanz
AppConfig g_config;

void config_init(void) {
    g_config.webserver_port = CONFIG_DEFAULT_WEB_PORT;
    g_config.log_level = CONFIG_DEFAULT_LOG_LEVEL;
    g_config.max_articles = CONFIG_DEFAULT_MAX_ARTICLES;
    g_config.max_string_length = CONFIG_DEFAULT_MAX_STRING_LENGTH;
}

void print_config(void) {
    printf("Aktuelle Konfiguration:\n");
    printf("  Webserver-Port      : %d\n", g_config.webserver_port);
    printf("  Log-Level           : %d\n", g_config.log_level);
    printf("  Maximale Artikelanzahl: %zu\n", g_config.max_articles);
    printf("  Maximale String-Länge : %zu\n", g_config.max_string_length);
}
