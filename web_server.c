#include "web_server.h"
#include "crud_database.h"
#include "userinput_pruefung.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <limits.h>
#include <stdarg.h>

#ifdef _WIN32
#define _WINSOCK_DEPRECATED_NO_WARNINGS

#include <winsock2.h>
#include <ws2tcpip.h>
typedef SOCKET socket_t;

#define close_socket closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
typedef int socket_t;
#define INVALID_SOCKET (-1)
#define SOCKET_ERROR (-1)
#define close_socket close
#endif

#ifndef DATA_DIRECTORY
#define DATA_DIRECTORY "data"
#endif

#ifndef WEB_DIRECTORY
#define WEB_DIRECTORY "web"
#endif

#ifndef SHOPPING_LIST_PATH
#define SHOPPING_LIST_PATH "einkaufsliste.txt"
#endif

#define SERVER_PORT 8081
#define MAX_REQUEST_SIZE 65536
#define MAX_PARAMS 64
#define MAX_KEY_LEN 64
#define MAX_VALUE_LEN 512
#define MAX_DB_FILES 128
#define SHOPPING_LIST_MAX_ITEMS 100
#define SHOPPING_LIST_MAX_LEN 256

/* ===== Grunddatenstrukturen ===== */

typedef struct {
    char schluessel[MAX_KEY_LEN];
    char wert[MAX_VALUE_LEN];
} AnfrageParameter;

typedef struct {
    char methode[8];
    char pfad[256];
    char abfrage[512];
    char inhalt_typ[64];
    size_t inhalt_laenge;
    AnfrageParameter abfrage_parameter[MAX_PARAMS];
    int abfrage_anzahl;
    AnfrageParameter body_parameter[MAX_PARAMS];
    int body_anzahl;
    char *body;
} HttpAnfrage;

typedef struct {
    char *daten;
    size_t laenge;
    size_t kapazitaet;
} Buffer;

typedef enum {
    MENGENART_UNBEKANNT,
    MENGENART_GRAMM,
    MENGENART_MILLILITER,
    MENGENART_STUECK
} Mengenart;

typedef struct {
    double menge;
    Mengenart art;
} Mengenangabe;

/* ===== Buffer-Helfer ===== */

static int buffer_initialisieren(Buffer *puffer) {
    puffer->daten = (char *)malloc(1024);
    if (puffer->daten == NULL) {
        return -1;
    }
    puffer->laenge = 0;
    puffer->kapazitaet = 1024;
    puffer->daten[0] = '\0';
    return 0;
}

static int buffer_reservieren(Buffer *puffer, size_t benoetigt) {
    if (puffer->laenge + benoetigt < puffer->kapazitaet) {
        return 0;
    }
    size_t neue_kapazitaet = puffer->kapazitaet;
    while (neue_kapazitaet < puffer->laenge + benoetigt + 1) {
        neue_kapazitaet *= 2;
    }
    char *neue_daten = (char *)realloc(puffer->daten, neue_kapazitaet);
    if (neue_daten == NULL) {
        return -1;
    }
    puffer->daten = neue_daten;
    puffer->kapazitaet = neue_kapazitaet;
    return 0;
}

static int buffer_anfuegen(Buffer *puffer, const char *daten, size_t datenlaenge) {
    if (buffer_reservieren(puffer, datenlaenge) != 0) {
        return -1;
    }
    memcpy(puffer->daten + puffer->laenge, daten, datenlaenge);
    puffer->laenge += datenlaenge;
    puffer->daten[puffer->laenge] = '\0';
    return 0;
}

static int buffer_anfuegen_text(Buffer *puffer, const char *text) {
    return buffer_anfuegen(puffer, text, strlen(text));
}

static int buffer_anfuegen_zeichen(Buffer *puffer, char zeichen) {
    return buffer_anfuegen(puffer, &zeichen, 1);
}

static int buffer_anfuegen_format(Buffer *puffer, const char *format, ...) {
    va_list argumente;
    va_start(argumente, format);
    char zwischenablage[512];
    int geschrieben = vsnprintf(zwischenablage, sizeof zwischenablage, format, argumente);
    va_end(argumente);
    if (geschrieben < 0) {
        return -1;
    }
    if ((size_t)geschrieben >= sizeof zwischenablage) {
        size_t benoetigt = (size_t)geschrieben + 1;
        char *dynamisch = (char *)malloc(benoetigt);
        if (dynamisch == NULL) {
            return -1;
        }
        va_start(argumente, format);
        vsnprintf(dynamisch, benoetigt, format, argumente);
        va_end(argumente);
        int resultat = buffer_anfuegen(puffer, dynamisch, (size_t)geschrieben);
        free(dynamisch);
        return resultat;
    }
    return buffer_anfuegen(puffer, zwischenablage, (size_t)geschrieben);
}

static void buffer_freigeben(Buffer *puffer) {
    free(puffer->daten);
    puffer->daten = NULL;
    puffer->laenge = 0;
    puffer->kapazitaet = 0;
}
/* ===== URL- und Parameterbearbeitung ===== */

static int hex_wert(char zeichen) {
    if (zeichen >= '0' && zeichen <= '9') {
        return zeichen - '0';
    }
    if (zeichen >= 'A' && zeichen <= 'F') {
        return zeichen - 'A' + 10;
    }
    if (zeichen >= 'a' && zeichen <= 'f') {
        return zeichen - 'a' + 10;
    }
    return -1;
}

static void url_dekodieren(const char *quelle, char *ziel, size_t zielgroesse) {
    size_t zi = 0;
    for (size_t i = 0; quelle[i] && zi + 1 < zielgroesse; ) {
        if (quelle[i] == '%' && quelle[i + 1] != '\0' && quelle[i + 2] != '\0') {
            int h1 = hex_wert(quelle[i + 1]);
            int h2 = hex_wert(quelle[i + 2]);
            if (h1 >= 0 && h2 >= 0) {
                ziel[zi++] = (char)((h1 << 4) | h2);
                i += 3;
                continue;
            }
        }
        if (quelle[i] == '+') {
            ziel[zi++] = ' ';
            i++;
        } else {
            ziel[zi++] = quelle[i++];
        }
    }
    ziel[zi] = '\0';
}

static void segment_kopieren_dekodieren(const char *start, size_t laenge,
                                        char *dekodiert, size_t dekodiert_groesse) {
    if (dekodiert_groesse == 0) {
        return;
    }
    size_t roh_kapazitaet = dekodiert_groesse * 3;
    if (roh_kapazitaet == 0) {
        dekodiert[0] = '\0';
        return;
    }
    size_t nutzbar = laenge;
    if (nutzbar >= roh_kapazitaet) {
        nutzbar = roh_kapazitaet - 1;
    }
    char roh[roh_kapazitaet];
    if (nutzbar > 0) {
        memcpy(roh, start, nutzbar);
    }
    roh[nutzbar] = '\0';
    url_dekodieren(roh, dekodiert, dekodiert_groesse);
}

static void parameter_setzen(AnfrageParameter *parameter, const char *schluessel, const char *wert) {
    strncpy(parameter->schluessel, schluessel, MAX_KEY_LEN - 1);
    parameter->schluessel[MAX_KEY_LEN - 1] = '\0';
    strncpy(parameter->wert, wert, MAX_VALUE_LEN - 1);
    parameter->wert[MAX_VALUE_LEN - 1] = '\0';
}

static int formular_parameter_auslesen(const char *daten, AnfrageParameter *parameter, int max_parameter) {
    if (daten == NULL) {
        return 0;
    }
    int anzahl = 0;
    const char *p = daten;
    while (*p && anzahl < max_parameter) {
        const char *amp = strchr(p, '&');
        const char *ende = amp ? amp : (p + strlen(p));
        const char *gleich = memchr(p, '=', (size_t)(ende - p));
        char schluessel_puffer[MAX_KEY_LEN];
        char wert_puffer[MAX_VALUE_LEN];
        if (gleich != NULL) {
            segment_kopieren_dekodieren(p, (size_t)(gleich - p), schluessel_puffer, sizeof schluessel_puffer);
            segment_kopieren_dekodieren(gleich + 1, (size_t)(ende - gleich - 1), wert_puffer, sizeof wert_puffer);
        } else {
            segment_kopieren_dekodieren(p, (size_t)(ende - p), schluessel_puffer, sizeof schluessel_puffer);
            wert_puffer[0] = '\0';
        }
        parameter_setzen(&parameter[anzahl], schluessel_puffer, wert_puffer);
        anzahl++;
        if (amp == NULL) {
            break;
        }
        p = amp + 1;
    }
    return anzahl;
}

static const char *parameter_suchen(const AnfrageParameter *parameter, int anzahl, const char *schluessel) {
    for (int i = 0; i < anzahl; i++) {
        if (strcmp(parameter[i].schluessel, schluessel) == 0) {
            return parameter[i].wert;
        }
    }
    return NULL;
}

static int praefix_vergleichen_ohne_fall(const char *text, const char *praefix) {
    while (*praefix) {
        if (*text == '\0') {
            return 0;
        }
        if (tolower((unsigned char)*text) != tolower((unsigned char)*praefix)) {
            return 0;
        }
        text++;
        praefix++;
    }
    return 1;
}

static void json_text_anhaengen(Buffer *puffer, const char *text) {
    buffer_anfuegen_zeichen(puffer, '"');
    for (const unsigned char *p = (const unsigned char *)text; *p; ++p) {
        unsigned char zeichen = *p;
        switch (zeichen) {
            case '\\': buffer_anfuegen_text(puffer, "\\\\"); break;
            case '"': buffer_anfuegen_text(puffer, "\\\""); break;
            case '\n': buffer_anfuegen_text(puffer, "\\n"); break;
            case '\r': buffer_anfuegen_text(puffer, "\\r"); break;
            case '\t': buffer_anfuegen_text(puffer, "\\t"); break;
            default:
                if (zeichen < 0x20) {
                    buffer_anfuegen_format(puffer, "\\u%04x", zeichen);
                } else {
                    buffer_anfuegen_zeichen(puffer, (char)zeichen);
                }
                break;
        }
    }
    buffer_anfuegen_zeichen(puffer, '"');
}
/* ===== Pfad- und Dateibearbeitung ===== */

static const char *basisname_bestimmen(const char *pfad) {
    const char *slash = strrchr(pfad, '/');
#ifdef _WIN32
    const char *backslash = strrchr(pfad, '\\');
    if (slash == NULL || (backslash != NULL && backslash > slash)) {
        slash = backslash;
    }
#endif
    return slash ? slash + 1 : pfad;
}

static int pfad_enthaelt_ruecksprung(const char *pfad) {
    if (pfad == NULL) {
        return 1;
    }
    if (strstr(pfad, "..") != NULL) {
        return 1;
    }
    if (strstr(pfad, "\\") != NULL) {
        return 1;
    }
    return 0;
}

static int statischer_pfad_erstellen(const char *relativ, char *ziel, size_t zielgroesse) {
    if (relativ == NULL || pfad_enthaelt_ruecksprung(relativ)) {
        return -1;
    }
    while (*relativ == '/') {
        relativ++;
    }
    if (*relativ == '\0') {
        relativ = "index.html";
    }
    int geschrieben = snprintf(ziel, zielgroesse, "%s/%s", WEB_DIRECTORY, relativ);
    if (geschrieben < 0 || (size_t)geschrieben >= zielgroesse) {
        return -1;
    }
    return 0;
}

static const char *inhaltstyp_fuer_pfad(const char *pfad) {
    struct {
        const char *endung;
        const char *typ;
    } typen[] = {
        {".html", "text/html; charset=utf-8"},
        {".css",  "text/css; charset=utf-8"},
        {".js",   "application/javascript; charset=utf-8"},
        {".json", "application/json; charset=utf-8"},
        {".png",  "image/png"},
        {".jpg",  "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".svg",  "image/svg+xml"},
    };

    const char *endung = strrchr(pfad, '.');
    if (endung == NULL) {
        return "application/octet-stream";
    }

    for (size_t i = 0; i < sizeof(typen) / sizeof(typen[0]); i++) {
        if (strcmp(endung, typen[i].endung) == 0) {
            return typen[i].typ;
        }
    }

    return "application/octet-stream";
}
/* ===== Antwortaufbau ===== */

static void antwort_mit_header_senden(socket_t client, const char *status, const char *inhaltstyp,
                                      const char *body, size_t body_laenge, const char *extra_header) {
    const char *extra = extra_header ? extra_header : "";
    char header[768];
    int header_laenge = snprintf(header, sizeof header,
                                 "HTTP/1.1 %s\r\n"
                                 "Content-Type: %s\r\n"
                                 "Content-Length: %zu\r\n"
                                 "Connection: close\r\n"
                                 "Access-Control-Allow-Origin: *\r\n"
                                 "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                                 "Access-Control-Allow-Headers: Content-Type\r\n"
                                 "%s"
                                 "\r\n",
                                 status, inhaltstyp, body_laenge, extra);
    if (header_laenge > 0) {
        send(client, header, header_laenge, 0);
    }
    if (body_laenge > 0 && body != NULL) {
        send(client, body, (int)body_laenge, 0);
    }
}

static void antwort_senden(socket_t client, const char *status, const char *inhaltstyp,
                           const char *body, size_t body_laenge) {
    antwort_mit_header_senden(client, status, inhaltstyp, body, body_laenge, NULL);
}

static void leere_antwort_senden(socket_t client, const char *status) {
    antwort_senden(client, status, "text/plain; charset=utf-8", "", 0);
}

static void json_antwort_senden(socket_t client, const char *status, const char *json, size_t laenge) {
    antwort_senden(client, status, "application/json; charset=utf-8", json, laenge);
}

static void json_text_senden(socket_t client, const char *status, const char *json) {
    json_antwort_senden(client, status, json, strlen(json));
}

static void fehler_senden(socket_t client, const char *status, const char *meldung) {
    Buffer puffer;
    if (buffer_initialisieren(&puffer) != 0) {
        json_text_senden(client, "500 Internal Server Error", "{\"error\":\"Interner Fehler\"}");
        return;
    }
    buffer_anfuegen_text(&puffer, "{\"error\":");
    json_text_anhaengen(&puffer, meldung ? meldung : "Unbekannter Fehler");
    buffer_anfuegen_zeichen(&puffer, '}');
    json_antwort_senden(client, status, puffer.daten, puffer.laenge);
    buffer_freigeben(&puffer);
}
/* ===== Datenbankpfade ===== */

static int datenbank_pfad_bestimmen(const char *name, char *ziel, size_t zielgroesse) {
    if (name == NULL || *name == '\0') {
        return -1;
    }
    char dateien[MAX_DB_FILES][DB_MAX_FILENAME];
    int anzahl = list_csv_files(DATA_DIRECTORY, dateien, MAX_DB_FILES);
    if (anzahl <= 0) {
        return -1;
    }
    for (int i = 0; i < anzahl; i++) {
        const char *basis = basisname_bestimmen(dateien[i]);
        if (strcmp(basis, name) == 0) {
            strncpy(ziel, dateien[i], zielgroesse - 1);
            ziel[zielgroesse - 1] = '\0';
            return 0;
        }
    }
    return -1;
}

/* ===== Hilfsfunktionen für Texte ===== */

static void leerraum_entfernen(char *text) {
    if (text == NULL) {
        return;
    }
    char *start = text;
    while (*start && isspace((unsigned char)*start)) {
        start++;
    }
    size_t laenge = strlen(start);
    memmove(text, start, laenge + 1);
    while (laenge > 0 && isspace((unsigned char)text[laenge - 1])) {
        text[--laenge] = '\0';
    }
}

static void listeneintrag_aufteilen(const char *zeile, char *artikel, size_t artikel_groesse,
                                     char *anbieter, size_t anbieter_groesse) {
    if (artikel_groesse > 0) {
        artikel[0] = '\0';
    }
    if (anbieter_groesse > 0) {
        anbieter[0] = '\0';
    }
    if (zeile == NULL) {
        return;
    }
    const char *trennzeichen = strstr(zeile, " - ");
    if (trennzeichen != NULL) {
        size_t artikel_len = (size_t)(trennzeichen - zeile);
        if (artikel_len >= artikel_groesse) {
            artikel_len = artikel_groesse - 1;
        }
        memcpy(artikel, zeile, artikel_len);
        artikel[artikel_len] = '\0';
        const char *anbieter_start = trennzeichen + 3;
        strncpy(anbieter, anbieter_start, anbieter_groesse - 1);
        anbieter[anbieter_groesse - 1] = '\0';
    } else {
        if (artikel_groesse > 0) {
            strncpy(artikel, zeile, artikel_groesse - 1);
            artikel[artikel_groesse - 1] = '\0';
        }
        if (anbieter_groesse > 0) {
            anbieter[0] = '\0';
        }
    }
    leerraum_entfernen(artikel);
    leerraum_entfernen(anbieter);
}

/* ===== Mengenberechnung ===== */

static int einheit_normalisieren(const char *einheit, Mengenart *art, double *faktor) {
    if (einheit == NULL || art == NULL || faktor == NULL) {
        return -1;
    }
    if (strcmp(einheit, "g") == 0) {
        *art = MENGENART_GRAMM;
        *faktor = 1.0;
        return 0;
    }
    if (strcmp(einheit, "kg") == 0) {
        *art = MENGENART_GRAMM;
        *faktor = 1000.0;
        return 0;
    }
    if (strcmp(einheit, "l") == 0) {
        *art = MENGENART_MILLILITER;
        *faktor = 1000.0;
        return 0;
    }
    if (strcmp(einheit, "ml") == 0) {
        *art = MENGENART_MILLILITER;
        *faktor = 1.0;
        return 0;
    }
    if (strcmp(einheit, "stk") == 0 || strcmp(einheit, "st") == 0 || strcmp(einheit, "stück") == 0) {
        *art = MENGENART_STUECK;
        *faktor = 1.0;
        return 0;
    }
    return -1;
}

static int eintrag_in_mengenangabe(const DatabaseEntry *eintrag, Mengenangabe *angabe) {
    if (eintrag == NULL || angabe == NULL) {
        return -1;
    }
    if (eintrag->menge_wert <= 0.0) {
        return -1;
    }
    Mengenart art = MENGENART_UNBEKANNT;
    double faktor = 0.0;
    if (einheit_normalisieren(eintrag->menge_einheit, &art, &faktor) != 0) {
        return -1;
    }
    angabe->menge = eintrag->menge_wert * faktor;
    angabe->art = art;
    return 0;
}

static const char *mengeneinheit_bezeichnung(Mengenart art) {
    switch (art) {
        case MENGENART_GRAMM: return "g";
        case MENGENART_MILLILITER: return "ml";
        case MENGENART_STUECK: return "Stück";
        default: return "";
    }
}

static void menge_json_anhaengen(Buffer *puffer, const DatabaseEntry *eintrag) {
    buffer_anfuegen_text(puffer, ",\"mengeWert\":");
    char mengen_text[32];
    formatiere_mengenwert(eintrag->menge_wert, mengen_text, sizeof mengen_text);
    buffer_anfuegen_format(puffer, "%s", mengen_text);
    buffer_anfuegen_text(puffer, ",\"mengeEinheit\":");
    json_text_anhaengen(puffer, eintrag->menge_einheit);
}
/* ===== HTTP-Verarbeitung ===== */

static int httpanfrage_analysieren(char *puffer, size_t laenge, HttpAnfrage *anfrage) {
    (void)laenge;
    memset(anfrage, 0, sizeof *anfrage);
    char *header_ende = strstr(puffer, "\r\n\r\n");
    if (header_ende == NULL) {
        return -1;
    }
    char *zeilenende = strstr(puffer, "\r\n");
    if (zeilenende == NULL) {
        return -1;
    }
    *zeilenende = '\0';
    char ziel[512];
    char version[32];
    if (sscanf(puffer, "%7s %511s %31s", anfrage->methode, ziel, version) != 3) {
        return -1;
    }
    char *abfrage = strchr(ziel, '?');
    if (abfrage != NULL) {
        *abfrage = '\0';
        strncpy(anfrage->abfrage, abfrage + 1, sizeof anfrage->abfrage - 1);
        anfrage->abfrage[sizeof anfrage->abfrage - 1] = '\0';
    }
    strncpy(anfrage->pfad, ziel, sizeof anfrage->pfad - 1);
    anfrage->pfad[sizeof anfrage->pfad - 1] = '\0';
    char *header = zeilenende + 2;
    while (header < header_ende) {
        char *naechste = strstr(header, "\r\n");
        if (naechste == NULL || header == naechste) {
            break;
        }
        *naechste = '\0';
        char *doppelpunkt = strchr(header, ':');
        if (doppelpunkt != NULL) {
            *doppelpunkt = '\0';
            char *wert = doppelpunkt + 1;
            while (*wert != '\0' && isspace((unsigned char)*wert)) {
                wert++;
            }
            if (praefix_vergleichen_ohne_fall(header, "Content-Type")) {
                strncpy(anfrage->inhalt_typ, wert, sizeof anfrage->inhalt_typ - 1);
                anfrage->inhalt_typ[sizeof anfrage->inhalt_typ - 1] = '\0';
            } else if (praefix_vergleichen_ohne_fall(header, "Content-Length")) {
                anfrage->inhalt_laenge = (size_t)strtoul(wert, NULL, 10);
            }
        }
        header = naechste + 2;
    }
    anfrage->body = header_ende + 4;
    if (anfrage->inhalt_laenge > 0) {
        anfrage->body[anfrage->inhalt_laenge] = '\0';
    }
    anfrage->abfrage_anzahl = formular_parameter_auslesen(anfrage->abfrage,
                                                         anfrage->abfrage_parameter, MAX_PARAMS);
    if (anfrage->inhalt_laenge > 0 &&
        praefix_vergleichen_ohne_fall(anfrage->inhalt_typ, "application/x-www-form-urlencoded")) {
        anfrage->body_anzahl = formular_parameter_auslesen(anfrage->body,
                                                           anfrage->body_parameter, MAX_PARAMS);
    }
    return 0;
}

static int httpanfrage_lesen(socket_t client, char *puffer, size_t puffer_groesse, size_t *ausgabe_laenge) {
    size_t gesamt = 0;
    size_t header_laenge = 0;
    size_t erwarteter_body = 0;
    int header_geparst = 0;
    while (gesamt + 1 < puffer_groesse) {
        int empfangen = recv(client, puffer + gesamt, (int)(puffer_groesse - gesamt - 1), 0);
        if (empfangen <= 0) {
            return -1;
        }
        gesamt += (size_t)empfangen;
        puffer[gesamt] = '\0';
        if (header_geparst == 0) {
            char *header_ende = strstr(puffer, "\r\n\r\n");
            if (header_ende != NULL) {
                header_geparst = 1;
                header_laenge = (size_t)(header_ende + 4 - puffer);
                char header_kopie[4096];
                size_t kopie_laenge = header_laenge < sizeof header_kopie - 1 ?
                                       header_laenge : sizeof header_kopie - 1;
                memcpy(header_kopie, puffer, kopie_laenge);
                header_kopie[kopie_laenge] = '\0';
                char *zeile = strstr(header_kopie, "\r\n");
                if (zeile != NULL) {
                    zeile += 2;
                }
                while (zeile != NULL && *zeile != '\0') {
                    char *naechste = strstr(zeile, "\r\n");
                    if (naechste == NULL) {
                        break;
                    }
                    if (zeile == naechste) {
                        break;
                    }
                    *naechste = '\0';
                    char *doppelpunkt = strchr(zeile, ':');
                    if (doppelpunkt != NULL) {
                        *doppelpunkt = '\0';
                        char *wert = doppelpunkt + 1;
                        while (*wert != '\0' && isspace((unsigned char)*wert)) {
                            wert++;
                        }
                        if (praefix_vergleichen_ohne_fall(zeile, "Content-Length")) {
                            erwarteter_body = (size_t)strtoul(wert, NULL, 10);
                        }
                    }
                    zeile = naechste + 2;
                }
                if (erwarteter_body == 0) {
                    break;
                }
                size_t vorhandener_body = gesamt > header_laenge ? gesamt - header_laenge : 0;
                if (vorhandener_body >= erwarteter_body) {
                    break;
                }
            }
        } else {
            size_t vorhandener_body = gesamt > header_laenge ? gesamt - header_laenge : 0;
            if (vorhandener_body >= erwarteter_body) {
                break;
            }
        }
    }
    if (header_geparst == 0) {
        return -1;
    }
    if (ausgabe_laenge != NULL) {
        *ausgabe_laenge = gesamt;
    }
    puffer[gesamt] = '\0';
    return 0;
}

static void dateiantwort_senden(socket_t client, const char *pfad) {
    FILE *datei = fopen(pfad, "rb");
    if (datei == NULL) {
        fehler_senden(client, "404 Not Found", "Datei nicht gefunden");
        return;
    }
    if (fseek(datei, 0, SEEK_END) != 0) {
        fclose(datei);
        fehler_senden(client, "500 Internal Server Error", "Dateizugriff fehlgeschlagen");
        return;
    }
    long groesse = ftell(datei);
    if (groesse < 0) {
        fclose(datei);
        fehler_senden(client, "500 Internal Server Error", "Dateigröße unbekannt");
        return;
    }
    rewind(datei);
    char *daten = (char *)malloc((size_t)groesse);
    if (daten == NULL) {
        fclose(datei);
        fehler_senden(client, "500 Internal Server Error", "Speicherfehler");
        return;
    }
    size_t gelesen = fread(daten, 1, (size_t)groesse, datei);
    fclose(datei);
    if (gelesen != (size_t)groesse) {
        free(daten);
        fehler_senden(client, "500 Internal Server Error", "Dateilesefehler");
        return;
    }
    const char *typ = inhaltstyp_fuer_pfad(pfad);
    antwort_senden(client, "200 OK", typ, daten, (size_t)groesse);
    free(daten);
}
/* ===== Datenbankoperationen ===== */

static int parameter_als_int(const char *wert, int *ziel) {
    if (wert == NULL) {
        return -1;
    }
    if (pruefe_ganzzahl_eingabe(wert, LONG_MIN, LONG_MAX) != 0) {
        return -1;
    }
    char *ende = NULL;
    long roh = strtol(wert, &ende, 10);
    if (ende == NULL || *ende != '\0') {
        return -1;
    }
    *ziel = (int)roh;
    return 0;
}

static int parameter_als_double(const char *wert, double *ziel) {
    if (wert == NULL) {
        return -1;
    }
    if (pruefe_dezimalzahl_eingabe(wert, -1e12, 1e12, 6) != 0) {
        return -1;
    }
    char puffer[64];
    strncpy(puffer, wert, sizeof puffer - 1);
    puffer[sizeof puffer - 1] = '\0';
    for (char *c = puffer; *c; ++c) {
        if (*c == ',') {
            *c = '.';
        }
    }
    char *ende = NULL;
    double roh = strtod(puffer, &ende);
    if (ende == NULL || *ende != '\0') {
        return -1;
    }
    *ziel = roh;
    return 0;
}

static DatabaseEntry *datenbank_eintrag_finden(Database *datenbank, int id) {
    if (datenbank == NULL) {
        return NULL;
    }
    for (int i = 0; i < datenbank->anzahl; i++) {
        if (datenbank->eintraege[i].id == id) {
            return &datenbank->eintraege[i];
        }
    }
    return NULL;
}

static int datenbank_index_finden(const Database *datenbank, int id) {
    if (datenbank == NULL) {
        return -1;
    }
    for (int i = 0; i < datenbank->anzahl; i++) {
        if (datenbank->eintraege[i].id == id) {
            return i;
        }
    }
    return -1;
}

static void handle_db_pflege(socket_t client, const HttpAnfrage *anfrage, int ist_aktualisierung) {
    const char *name = parameter_suchen(anfrage->body_parameter, anfrage->body_anzahl, "name");
    const char *id_text = parameter_suchen(anfrage->body_parameter, anfrage->body_anzahl, "id");
    const char *artikel = parameter_suchen(anfrage->body_parameter, anfrage->body_anzahl, "artikel");
    const char *anbieter = parameter_suchen(anfrage->body_parameter, anfrage->body_anzahl, "anbieter");
    const char *preis_text = parameter_suchen(anfrage->body_parameter, anfrage->body_anzahl, "preisCent");
    const char *menge_wert_text = parameter_suchen(anfrage->body_parameter, anfrage->body_anzahl, "mengeWert");
    const char *menge_einheit_text = parameter_suchen(anfrage->body_parameter, anfrage->body_anzahl, "mengeEinheit");
    if (name == NULL || id_text == NULL || artikel == NULL || anbieter == NULL || preis_text == NULL ||
        menge_wert_text == NULL || menge_einheit_text == NULL) {
        fehler_senden(client, "400 Bad Request", "Parameter fehlen");
        return;
    }
    if (pruefe_dateiname(name, DB_MAX_FILENAME - 1) != 0) {
        fehler_senden(client, "400 Bad Request", "Ungültiger Datenbankname");
        return;
    }
    if (pruefe_text_eingabe(artikel, DB_MAX_TEXT - 1) != 0) {
        fehler_senden(client, "400 Bad Request", "Ungültiger Artikeltext");
        return;
    }
    if (pruefe_text_eingabe(anbieter, DB_MAX_TEXT - 1) != 0) {
        fehler_senden(client, "400 Bad Request", "Ungültiger Anbietertext");
        return;
    }
    if (pruefe_text_eingabe(menge_einheit_text, sizeof(((DatabaseEntry *)0)->menge_einheit) - 1) != 0) {
        fehler_senden(client, "400 Bad Request", "Ungültige Mengeneinheit");
        return;
    }
    int id = 0;
    if (parameter_als_int(id_text, &id) != 0 || id < 0) {
        fehler_senden(client, "400 Bad Request", "Ungültige ID");
        return;
    }
    int preis = 0;
    if (parameter_als_int(preis_text, &preis) != 0 || preis < 0) {
        fehler_senden(client, "400 Bad Request", "Ungültiger Preis");
        return;
    }
    double mengenwert_roh = 0.0;
    if (parameter_als_double(menge_wert_text, &mengenwert_roh) != 0 || mengenwert_roh <= 0.0) {
        fehler_senden(client, "400 Bad Request", "Ungültiger Mengenwert");
        return;
    }
    double mengenwert = mengenwert_roh;
    char menge_einheit_norm[sizeof(((DatabaseEntry *)0)->menge_einheit)];
    if (normalisiere_mengeneinheit(menge_einheit_text, menge_einheit_norm,
                                   sizeof menge_einheit_norm, &mengenwert) != 0) {
        fehler_senden(client, "400 Bad Request", "Ungültige Einheit");
        return;
    }
    char pfad[DB_MAX_FILENAME];
    if (datenbank_pfad_bestimmen(name, pfad, sizeof pfad) != 0) {
        fehler_senden(client, "404 Not Found", "Datenbank nicht gefunden");
        return;
    }
    Database datenbank;
    if (load_database(pfad, &datenbank) != 0) {
        fehler_senden(client, "500 Internal Server Error", "Datenbank konnte nicht geladen werden");
        return;
    }
    DatabaseEntry *eintrag = datenbank_eintrag_finden(&datenbank, id);
    if (ist_aktualisierung) {
        if (eintrag == NULL) {
            fehler_senden(client, "404 Not Found", "Eintrag nicht gefunden");
            return;
        }
    } else {
        if (eintrag != NULL) {
            fehler_senden(client, "400 Bad Request", "ID existiert bereits");
            return;
        }
        if (datenbank.anzahl >= DB_MAX_ENTRIES) {
            fehler_senden(client, "400 Bad Request", "Keine freien Plätze");
            return;
        }
        eintrag = &datenbank.eintraege[datenbank.anzahl++];
        eintrag->id = id;
    }
    strncpy(eintrag->artikel, artikel, DB_MAX_TEXT - 1);
    eintrag->artikel[DB_MAX_TEXT - 1] = '\0';
    strncpy(eintrag->anbieter, anbieter, DB_MAX_TEXT - 1);
    eintrag->anbieter[DB_MAX_TEXT - 1] = '\0';
    eintrag->preis_ct = preis;
    eintrag->menge_wert = mengenwert;
    strncpy(eintrag->menge_einheit, menge_einheit_norm, sizeof(eintrag->menge_einheit) - 1);
    eintrag->menge_einheit[sizeof(eintrag->menge_einheit) - 1] = '\0';
    if (save_database(&datenbank) != 0) {
        fehler_senden(client, "500 Internal Server Error", "Speichern fehlgeschlagen");
        return;
    }
    Buffer puffer;
    if (buffer_initialisieren(&puffer) != 0) {
        fehler_senden(client, "500 Internal Server Error", "Speicherfehler");
        return;
    }
    buffer_anfuegen_text(&puffer, "{\"status\":\"ok\",\"entry\":{");
    buffer_anfuegen_text(&puffer, "\"id\":");
    buffer_anfuegen_format(&puffer, "%d", eintrag->id);
    buffer_anfuegen_text(&puffer, ",\"artikel\":");
    json_text_anhaengen(&puffer, eintrag->artikel);
    buffer_anfuegen_text(&puffer, ",\"anbieter\":");
    json_text_anhaengen(&puffer, eintrag->anbieter);
    buffer_anfuegen_text(&puffer, ",\"preisCent\":");
    buffer_anfuegen_format(&puffer, "%d", eintrag->preis_ct);
    menge_json_anhaengen(&puffer, eintrag);
    buffer_anfuegen_text(&puffer, "}}");
    json_antwort_senden(client, "200 OK", puffer.daten, puffer.laenge);
    buffer_freigeben(&puffer);
}

static void handle_db_loeschen(socket_t client, const HttpAnfrage *anfrage) {
    const char *name = parameter_suchen(anfrage->body_parameter, anfrage->body_anzahl, "name");
    const char *id_text = parameter_suchen(anfrage->body_parameter, anfrage->body_anzahl, "id");
    if (name == NULL || id_text == NULL) {
        fehler_senden(client, "400 Bad Request", "Parameter fehlen");
        return;
    }
    if (pruefe_dateiname(name, DB_MAX_FILENAME - 1) != 0) {
        fehler_senden(client, "400 Bad Request", "Ungültiger Datenbankname");
        return;
    }
    int id = 0;
    if (parameter_als_int(id_text, &id) != 0) {
        fehler_senden(client, "400 Bad Request", "Ungültige ID");
        return;
    }
    char pfad[DB_MAX_FILENAME];
    if (datenbank_pfad_bestimmen(name, pfad, sizeof pfad) != 0) {
        fehler_senden(client, "404 Not Found", "Datenbank nicht gefunden");
        return;
    }
    Database datenbank;
    if (load_database(pfad, &datenbank) != 0) {
        fehler_senden(client, "500 Internal Server Error", "Datenbank konnte nicht geladen werden");
        return;
    }
    int index = datenbank_index_finden(&datenbank, id);
    if (index < 0) {
        fehler_senden(client, "404 Not Found", "Eintrag nicht gefunden");
        return;
    }
    for (int i = index + 1; i < datenbank.anzahl; i++) {
        datenbank.eintraege[i - 1] = datenbank.eintraege[i];
    }
    datenbank.anzahl--;
    if (save_database(&datenbank) != 0) {
        fehler_senden(client, "500 Internal Server Error", "Speichern fehlgeschlagen");
        return;
    }
    json_text_senden(client, "200 OK", "{\"status\":\"ok\"}");
}
/* ===== Einkaufsliste ===== */

static int einkaufsliste_laden(char eintraege[SHOPPING_LIST_MAX_ITEMS][SHOPPING_LIST_MAX_LEN]) {
    FILE *datei = fopen(SHOPPING_LIST_PATH, "r");
    if (datei == NULL) {
        return 0;
    }
    int anzahl = 0;
    char zeile[SHOPPING_LIST_MAX_LEN];
    while (anzahl < SHOPPING_LIST_MAX_ITEMS && fgets(zeile, sizeof zeile, datei) != NULL) {
        size_t len = strlen(zeile);
        while (len > 0 && (zeile[len - 1] == '\n' || zeile[len - 1] == '\r')) {
            zeile[--len] = '\0';
        }
        strncpy(eintraege[anzahl], zeile, SHOPPING_LIST_MAX_LEN - 1);
        eintraege[anzahl][SHOPPING_LIST_MAX_LEN - 1] = '\0';
        anzahl++;
    }
    fclose(datei);
    return anzahl;
}

static int einkaufsliste_speichern(char eintraege[SHOPPING_LIST_MAX_ITEMS][SHOPPING_LIST_MAX_LEN], int anzahl) {
    FILE *datei = fopen(SHOPPING_LIST_PATH, "w");
    if (datei == NULL) {
        return -1;
    }
    for (int i = 0; i < anzahl; i++) {
        if (fprintf(datei, "%s\n", eintraege[i]) < 0) {
            fclose(datei);
            return -1;
        }
    }
    fclose(datei);
    return 0;
}

static void listeneintrag_erzeugen(const char *artikel, const char *anbieter,
                                   char *ziel, size_t zielgroesse) {
    if (zielgroesse == 0) {
        return;
    }
    if (anbieter != NULL && *anbieter) {
        snprintf(ziel, zielgroesse, "%s - %s", artikel, anbieter);
    } else {
        snprintf(ziel, zielgroesse, "%s", artikel);
    }
    ziel[zielgroesse - 1] = '\0';
}

static void handle_liste_pflege(socket_t client, const HttpAnfrage *anfrage, int ist_aktualisierung) {
    const char *artikel = parameter_suchen(anfrage->body_parameter, anfrage->body_anzahl, "artikel");
    const char *anbieter = parameter_suchen(anfrage->body_parameter, anfrage->body_anzahl, "anbieter");
    const char *index_text = parameter_suchen(anfrage->body_parameter, anfrage->body_anzahl, "index");
    if (artikel == NULL || pruefe_text_eingabe(artikel, SHOPPING_LIST_MAX_LEN - 1) != 0) {
        fehler_senden(client, "400 Bad Request", "Ungültiger Artikel");
        return;
    }
    if (anbieter != NULL && pruefe_text_eingabe(anbieter, SHOPPING_LIST_MAX_LEN - 1) != 0) {
        fehler_senden(client, "400 Bad Request", "Ungültiger Anbieter");
        return;
    }
    char eintraege[SHOPPING_LIST_MAX_ITEMS][SHOPPING_LIST_MAX_LEN];
    int anzahl = einkaufsliste_laden(eintraege);
    if (ist_aktualisierung) {
        if (index_text == NULL) {
            fehler_senden(client, "400 Bad Request", "Index fehlt");
            return;
        }
        int index = 0;
        if (parameter_als_int(index_text, &index) != 0 || index < 0 || index >= anzahl) {
            fehler_senden(client, "400 Bad Request", "Ungültiger Index");
            return;
        }
        listeneintrag_erzeugen(artikel, anbieter ? anbieter : "", eintraege[index], sizeof eintraege[index]);
    } else {
        if (anzahl >= SHOPPING_LIST_MAX_ITEMS) {
            fehler_senden(client, "400 Bad Request", "Liste voll");
            return;
        }
        listeneintrag_erzeugen(artikel, anbieter ? anbieter : "", eintraege[anzahl], sizeof eintraege[anzahl]);
        anzahl++;
    }
    if (einkaufsliste_speichern(eintraege, anzahl) != 0) {
        fehler_senden(client, "500 Internal Server Error", "Speichern fehlgeschlagen");
        return;
    }
    json_text_senden(client, "200 OK", "{\"status\":\"ok\"}");
}

static void handle_liste_loeschen(socket_t client, const HttpAnfrage *anfrage) {
    const char *index_text = parameter_suchen(anfrage->body_parameter, anfrage->body_anzahl, "index");
    if (index_text == NULL) {
        fehler_senden(client, "400 Bad Request", "Index fehlt");
        return;
    }
    int index = 0;
    if (parameter_als_int(index_text, &index) != 0) {
        fehler_senden(client, "400 Bad Request", "Ungültiger Index");
        return;
    }
    char eintraege[SHOPPING_LIST_MAX_ITEMS][SHOPPING_LIST_MAX_LEN];
    int anzahl = einkaufsliste_laden(eintraege);
    if (index < 0 || index >= anzahl) {
        fehler_senden(client, "404 Not Found", "Eintrag nicht gefunden");
        return;
    }
    for (int i = index + 1; i < anzahl; i++) {
        strncpy(eintraege[i - 1], eintraege[i], SHOPPING_LIST_MAX_LEN - 1);
        eintraege[i - 1][SHOPPING_LIST_MAX_LEN - 1] = '\0';
    }
    anzahl--;
    if (einkaufsliste_speichern(eintraege, anzahl) != 0) {
        fehler_senden(client, "500 Internal Server Error", "Speichern fehlgeschlagen");
        return;
    }
    json_text_senden(client, "200 OK", "{\"status\":\"ok\"}");
}
/* ===== Preisvergleich ===== */

static void handle_vergleich_einzel(socket_t client, const HttpAnfrage *anfrage) {
    const char *name = parameter_suchen(anfrage->body_parameter, anfrage->body_anzahl, "name");
    const char *erste_id_text = parameter_suchen(anfrage->body_parameter, anfrage->body_anzahl, "firstId");
    const char *zweite_id_text = parameter_suchen(anfrage->body_parameter, anfrage->body_anzahl, "secondId");
    const char *menge_text = parameter_suchen(anfrage->body_parameter, anfrage->body_anzahl, "amount");
    if (name == NULL || erste_id_text == NULL || zweite_id_text == NULL || menge_text == NULL) {
        fehler_senden(client, "400 Bad Request", "Parameter fehlen");
        return;
    }
    if (pruefe_dateiname(name, DB_MAX_FILENAME - 1) != 0) {
        fehler_senden(client, "400 Bad Request", "Ungültiger Datenbankname");
        return;
    }
    int erste_id = 0;
    int zweite_id = 0;
    double menge = 0.0;
    if (parameter_als_int(erste_id_text, &erste_id) != 0 || parameter_als_int(zweite_id_text, &zweite_id) != 0) {
        fehler_senden(client, "400 Bad Request", "Ungültige IDs");
        return;
    }
    if (erste_id < 0 || zweite_id < 0) {
        fehler_senden(client, "400 Bad Request", "Ungültige IDs");
        return;
    }
    if (parameter_als_double(menge_text, &menge) != 0 || menge <= 0.0) {
        fehler_senden(client, "400 Bad Request", "Ungültige Menge");
        return;
    }
    char pfad[DB_MAX_FILENAME];
    if (datenbank_pfad_bestimmen(name, pfad, sizeof pfad) != 0) {
        fehler_senden(client, "404 Not Found", "Datenbank nicht gefunden");
        return;
    }
    Database datenbank;
    if (load_database(pfad, &datenbank) != 0) {
        fehler_senden(client, "500 Internal Server Error", "Datenbank konnte nicht geladen werden");
        return;
    }
    DatabaseEntry *erste = datenbank_eintrag_finden(&datenbank, erste_id);
    DatabaseEntry *zweite = datenbank_eintrag_finden(&datenbank, zweite_id);
    if (erste == NULL || zweite == NULL) {
        fehler_senden(client, "404 Not Found", "Eintrag nicht gefunden");
        return;
    }
    Mengenangabe menge_erste;
    Mengenangabe menge_zweite;
    if (eintrag_in_mengenangabe(erste, &menge_erste) != 0 || eintrag_in_mengenangabe(zweite, &menge_zweite) != 0) {
        fehler_senden(client, "400 Bad Request", "Mengenangaben können nicht interpretiert werden");
        return;
    }
    if (menge_erste.art != menge_zweite.art) {
        fehler_senden(client, "400 Bad Request", "Einheiten sind nicht kompatibel");
        return;
    }
    double preis_pro_einheit_erste = (double)erste->preis_ct / menge_erste.menge;
    double preis_pro_einheit_zweite = (double)zweite->preis_ct / menge_zweite.menge;
    double gesamt_erste = preis_pro_einheit_erste * menge;
    double gesamt_zweite = preis_pro_einheit_zweite * menge;
    const char *einheit = mengeneinheit_bezeichnung(menge_erste.art);
    const double epsilon = 1e-6;
    const char *guenstiger = "equal";
    if (preis_pro_einheit_erste + epsilon < preis_pro_einheit_zweite) {
        guenstiger = "first";
    } else if (preis_pro_einheit_zweite + epsilon < preis_pro_einheit_erste) {
        guenstiger = "second";
    }
    Buffer puffer;
    if (buffer_initialisieren(&puffer) != 0) {
        fehler_senden(client, "500 Internal Server Error", "Speicherfehler");
        return;
    }
    buffer_anfuegen_text(&puffer, "{\"status\":\"ok\",\"unit\":");
    json_text_anhaengen(&puffer, einheit);
    buffer_anfuegen_text(&puffer, ",\"amount\":");
    buffer_anfuegen_format(&puffer, "%.4f", menge);
    buffer_anfuegen_text(&puffer, ",\"cheaper\":");
    json_text_anhaengen(&puffer, guenstiger);
    buffer_anfuegen_text(&puffer, ",\"first\":{");
    buffer_anfuegen_text(&puffer, "\"id\":");
    buffer_anfuegen_format(&puffer, "%d", erste->id);
    buffer_anfuegen_text(&puffer, ",\"artikel\":");
    json_text_anhaengen(&puffer, erste->artikel);
    buffer_anfuegen_text(&puffer, ",\"anbieter\":");
    json_text_anhaengen(&puffer, erste->anbieter);
    menge_json_anhaengen(&puffer, erste);
    buffer_anfuegen_text(&puffer, ",\"preisCent\":");
    buffer_anfuegen_format(&puffer, "%d", erste->preis_ct);
    buffer_anfuegen_text(&puffer, ",\"unitPrice\":");
    buffer_anfuegen_format(&puffer, "%.6f", preis_pro_einheit_erste);
    buffer_anfuegen_text(&puffer, ",\"total\":");
    buffer_anfuegen_format(&puffer, "%.6f", gesamt_erste);
    buffer_anfuegen_text(&puffer, "},\"second\":{");
    buffer_anfuegen_text(&puffer, "\"id\":");
    buffer_anfuegen_format(&puffer, "%d", zweite->id);
    buffer_anfuegen_text(&puffer, ",\"artikel\":");
    json_text_anhaengen(&puffer, zweite->artikel);
    buffer_anfuegen_text(&puffer, ",\"anbieter\":");
    json_text_anhaengen(&puffer, zweite->anbieter);
    menge_json_anhaengen(&puffer, zweite);
    buffer_anfuegen_text(&puffer, ",\"preisCent\":");
    buffer_anfuegen_format(&puffer, "%d", zweite->preis_ct);
    buffer_anfuegen_text(&puffer, ",\"unitPrice\":");
    buffer_anfuegen_format(&puffer, "%.6f", preis_pro_einheit_zweite);
    buffer_anfuegen_text(&puffer, ",\"total\":");
    buffer_anfuegen_format(&puffer, "%.6f", gesamt_zweite);
    buffer_anfuegen_text(&puffer, "}}");
    json_antwort_senden(client, "200 OK", puffer.daten, puffer.laenge);
    buffer_freigeben(&puffer);
}

static void handle_listenvergleich(socket_t client, const HttpAnfrage *anfrage) {
    const char *name = parameter_suchen(anfrage->body_parameter, anfrage->body_anzahl, "name");
    const char *anwenden_text = parameter_suchen(anfrage->body_parameter, anfrage->body_anzahl, "apply");
    if (name == NULL || *name == '\0') {
        fehler_senden(client, "400 Bad Request", "Parameter 'name' fehlt");
        return;
    }
    if (pruefe_dateiname(name, DB_MAX_FILENAME - 1) != 0) {
        fehler_senden(client, "400 Bad Request", "Ungültiger Datenbankname");
        return;
    }
    if (pruefe_flag_eingabe(anwenden_text) != 0) {
        fehler_senden(client, "400 Bad Request", "Ungültiger Schalterwert");
        return;
    }
    int anwenden = (anwenden_text != NULL && strcmp(anwenden_text, "1") == 0) ? 1 : 0;
    char pfad[DB_MAX_FILENAME];
    if (datenbank_pfad_bestimmen(name, pfad, sizeof pfad) != 0) {
        fehler_senden(client, "404 Not Found", "Datenbank nicht gefunden");
        return;
    }
    Database datenbank;
    if (load_database(pfad, &datenbank) != 0) {
        fehler_senden(client, "500 Internal Server Error", "Datenbank konnte nicht geladen werden");
        return;
    }
    char eintraege[SHOPPING_LIST_MAX_ITEMS][SHOPPING_LIST_MAX_LEN];
    int anzahl = einkaufsliste_laden(eintraege);
    Buffer puffer;
    if (buffer_initialisieren(&puffer) != 0) {
        fehler_senden(client, "500 Internal Server Error", "Speicherfehler");
        return;
    }
    buffer_anfuegen_text(&puffer, "{\"status\":\"ok\",\"updated\":");
    buffer_anfuegen_text(&puffer, anwenden ? "true" : "false");
    buffer_anfuegen_text(&puffer, ",\"items\":[");
    int geaendert = 0;
    for (int i = 0; i < anzahl; i++) {
        if (i > 0) {
            buffer_anfuegen_zeichen(&puffer, ',');
        }
        char artikel[DB_MAX_TEXT];
        char anbieter[DB_MAX_TEXT];
        listeneintrag_aufteilen(eintraege[i], artikel, sizeof artikel, anbieter, sizeof anbieter);
        buffer_anfuegen_text(&puffer, "{\"index\":");
        buffer_anfuegen_format(&puffer, "%d", i);
        buffer_anfuegen_text(&puffer, ",\"artikel\":");
        json_text_anhaengen(&puffer, artikel);
        buffer_anfuegen_text(&puffer, ",\"aktuellerAnbieter\":");
        json_text_anhaengen(&puffer, anbieter);
        int treffer = 0;
        int bester_index = -1;
        int bester_hat_menge = 0;
        double bester_preis_einheit = 0.0;
        int bester_preis_packung = 0;
        for (int j = 0; j < datenbank.anzahl; j++) {
            DatabaseEntry *eintrag = &datenbank.eintraege[j];
            if (strcmp(eintrag->artikel, artikel) != 0) {
                continue;
            }
            treffer++;
            Mengenangabe menge_angabe;
            int hat_menge = 0;
            double preis_einheit = 0.0;
            if (eintrag_in_mengenangabe(eintrag, &menge_angabe) == 0 && menge_angabe.menge > 0.0) {
                hat_menge = 1;
                preis_einheit = (double)eintrag->preis_ct / menge_angabe.menge;
            }
            if (hat_menge) {
                if (bester_hat_menge == 0 || preis_einheit + 1e-6 < bester_preis_einheit) {
                    bester_index = j;
                    bester_hat_menge = 1;
                    bester_preis_einheit = preis_einheit;
                    bester_preis_packung = eintrag->preis_ct;
                }
            } else {
                if (bester_index == -1 || (bester_hat_menge == 0 && eintrag->preis_ct < bester_preis_packung)) {
                    bester_index = j;
                    bester_preis_packung = eintrag->preis_ct;
                }
            }
        }
        buffer_anfuegen_text(&puffer, ",\"treffer\":");
        buffer_anfuegen_format(&puffer, "%d", treffer);
        if (bester_index >= 0) {
            DatabaseEntry *bester = &datenbank.eintraege[bester_index];
            buffer_anfuegen_text(&puffer, ",\"empfehlung\":{");
            buffer_anfuegen_text(&puffer, "\"anbieter\":");
            json_text_anhaengen(&puffer, bester->anbieter);
            buffer_anfuegen_text(&puffer, ",\"preisCent\":");
            buffer_anfuegen_format(&puffer, "%d", bester->preis_ct);
            menge_json_anhaengen(&puffer, bester);
            buffer_anfuegen_text(&puffer, "}");
            if (anwenden && strcmp(anbieter, bester->anbieter) != 0) {
                listeneintrag_erzeugen(artikel, bester->anbieter, eintraege[i], sizeof eintraege[i]);
                geaendert = 1;
            }
        } else {
            buffer_anfuegen_text(&puffer, ",\"empfehlung\":null");
        }
        buffer_anfuegen_text(&puffer, "}");
    }
    buffer_anfuegen_text(&puffer, "]}");
    json_antwort_senden(client, "200 OK", puffer.daten, puffer.laenge);
    buffer_freigeben(&puffer);
    if (anwenden && geaendert) {
        einkaufsliste_speichern(eintraege, anzahl);
    }
}
/* ===== Routing ===== */

static void route_anfrage(socket_t client, const HttpAnfrage *anfrage) {
    if (strcmp(anfrage->methode, "OPTIONS") == 0) {
        leere_antwort_senden(client, "204 No Content");
        return;
    }
    if (strcmp(anfrage->methode, "GET") == 0) {
        char pfad[512];
        if (statischer_pfad_erstellen(anfrage->pfad, pfad, sizeof pfad) != 0) {
            fehler_senden(client, "400 Bad Request", "Pfad ungültig");
            return;
        }
        FILE *probe = fopen(pfad, "rb");
        if (probe) {
            fclose(probe);
            dateiantwort_senden(client, pfad);
            return;
        }
        const char *name = parameter_suchen(anfrage->abfrage_parameter, anfrage->abfrage_anzahl, "name");
        if (name == NULL) {
            fehler_senden(client, "404 Not Found", "Pfad nicht gefunden");
            return;
        }
        if (pruefe_dateiname(name, DB_MAX_FILENAME - 1) != 0) {
            fehler_senden(client, "400 Bad Request", "Ungültiger Datenbankname");
            return;
        }
        char datenbank_pfad[DB_MAX_FILENAME];
        if (datenbank_pfad_bestimmen(name, datenbank_pfad, sizeof datenbank_pfad) != 0) {
            fehler_senden(client, "404 Not Found", "Datenbank nicht gefunden");
            return;
        }
        Database datenbank;
        if (load_database(datenbank_pfad, &datenbank) != 0) {
            fehler_senden(client, "500 Internal Server Error", "Datenbank konnte nicht geladen werden");
            return;
        }
        Buffer puffer;
        if (buffer_initialisieren(&puffer) != 0) {
            fehler_senden(client, "500 Internal Server Error", "Speicherfehler");
            return;
        }
        buffer_anfuegen_text(&puffer, "{\"status\":\"ok\",\"entries\":[");
        for (int i = 0; i < datenbank.anzahl; i++) {
            if (i > 0) {
                buffer_anfuegen_zeichen(&puffer, ',');
            }
            DatabaseEntry *eintrag = &datenbank.eintraege[i];
            buffer_anfuegen_text(&puffer, "{\"id\":");
            buffer_anfuegen_format(&puffer, "%d", eintrag->id);
            buffer_anfuegen_text(&puffer, ",\"artikel\":");
            json_text_anhaengen(&puffer, eintrag->artikel);
            buffer_anfuegen_text(&puffer, ",\"anbieter\":");
            json_text_anhaengen(&puffer, eintrag->anbieter);
            buffer_anfuegen_text(&puffer, ",\"preisCent\":");
            buffer_anfuegen_format(&puffer, "%d", eintrag->preis_ct);
            buffer_anfuegen_text(&puffer, ",\"preisEuro\":");
            buffer_anfuegen_format(&puffer, "%.2f", eintrag->preis_ct / 100.0);
            menge_json_anhaengen(&puffer, eintrag);
            buffer_anfuegen_text(&puffer, "}");
        }
        buffer_anfuegen_text(&puffer, "]}");
        json_antwort_senden(client, "200 OK", puffer.daten, puffer.laenge);
        buffer_freigeben(&puffer);
        return;
    }
    if (strcmp(anfrage->methode, "POST") == 0) {
        if (strcmp(anfrage->pfad, "/api/db/add") == 0) {
            handle_db_pflege(client, anfrage, 0);
            return;
        }
        if (strcmp(anfrage->pfad, "/api/db/update") == 0) {
            handle_db_pflege(client, anfrage, 1);
            return;
        }
        if (strcmp(anfrage->pfad, "/api/db/delete") == 0) {
            handle_db_loeschen(client, anfrage);
            return;
        }
        if (strcmp(anfrage->pfad, "/api/list/add") == 0) {
            handle_liste_pflege(client, anfrage, 0);
            return;
        }
        if (strcmp(anfrage->pfad, "/api/list/update") == 0) {
            handle_liste_pflege(client, anfrage, 1);
            return;
        }
        if (strcmp(anfrage->pfad, "/api/list/delete") == 0) {
            handle_liste_loeschen(client, anfrage);
            return;
        }
        if (strcmp(anfrage->pfad, "/api/compare/single") == 0) {
            handle_vergleich_einzel(client, anfrage);
            return;
        }
        if (strcmp(anfrage->pfad, "/api/compare/list") == 0) {
            handle_listenvergleich(client, anfrage);
            return;
        }
        fehler_senden(client, "404 Not Found", "Pfad nicht gefunden");
        return;
    }
    fehler_senden(client, "405 Method Not Allowed", "Methode nicht erlaubt");
}
/* ===== Clientbearbeitung ===== */

static void handle_clientverbindung(socket_t client) {
    char puffer[MAX_REQUEST_SIZE];
    size_t laenge = 0;
    if (httpanfrage_lesen(client, puffer, sizeof puffer, &laenge) != 0) {
        fehler_senden(client, "400 Bad Request", "Anfrage konnte nicht gelesen werden");
        return;
    }
    HttpAnfrage anfrage;
    if (httpanfrage_analysieren(puffer, laenge, &anfrage) != 0) {
        fehler_senden(client, "400 Bad Request", "Anfrage ist ungültig");
        return;
    }
    route_anfrage(client, &anfrage);
}

/* ===== Serverstart ===== */

static int socket_initialisieren(socket_t *ausgabe_socket) {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        return -1;
    }
#endif
    socket_t server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) {
#ifdef _WIN32
        WSACleanup();
#endif
        return -1;
    }
    int option = 1;
#ifdef _WIN32
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char *)&option, sizeof option);
#else
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &option, sizeof option);
#endif
    struct sockaddr_in adresse;
    memset(&adresse, 0, sizeof adresse);
    adresse.sin_family = AF_INET;
    adresse.sin_addr.s_addr = htonl(INADDR_ANY);
    adresse.sin_port = htons(SERVER_PORT);
    if (bind(server, (struct sockaddr *)&adresse, sizeof adresse) == SOCKET_ERROR) {
        close_socket(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return -1;
    }
    if (listen(server, 16) == SOCKET_ERROR) {
        close_socket(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return -1;
    }
    *ausgabe_socket = server;
    return 0;
}

int server_starten(void) {
    socket_t server;
    if (socket_initialisieren(&server) != 0) {
        fprintf(stderr, "Server konnte nicht gestartet werden\n");
        return 1;
    }
    printf("Server läuft auf http://localhost:%d\n", SERVER_PORT);
    for (;;) {
        struct sockaddr_in client_adresse;
        socklen_t adresslaenge = sizeof client_adresse;
        socket_t client = accept(server, (struct sockaddr *)&client_adresse, &adresslaenge);
        if (client == INVALID_SOCKET) {
            continue;
        }
        handle_clientverbindung(client);
        close_socket(client);
    }
    close_socket(server);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
