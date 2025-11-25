#ifndef CONFIG_H
#define CONFIG_H

#include <stddef.h>

// Standard-Konfigurationswerte
#define CONFIG_DEFAULT_WEB_PORT 8081
#define CONFIG_DEFAULT_LOG_LEVEL 2
#define CONFIG_DEFAULT_MAX_ARTICLES 500
#define CONFIG_DEFAULT_MAX_STRING_LENGTH 256

// Globale Konfigurationsstruktur
typedef struct {
    int webserver_port;        // TCP-Port des Webservers
    int log_level;             // Log-Level: 0=ERROR, 1=WARN, 2=INFO, 3=DEBUG
    size_t max_articles;       // Maximale Anzahl verwalteter Artikel
    size_t max_string_length;  // Maximale Länge generischer Strings
} AppConfig;

// Globaler Konfigurationszustand (wird in config.c definiert)
extern AppConfig g_config;

// Initialisiert die Konfiguration mit Standardwerten
void config_init(void);

// Gibt die aktuelle Konfiguration formatiert auf stdout aus
void print_config(void);

#endif // CONFIG_H
