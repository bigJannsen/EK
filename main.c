#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "druck_einkauf.h"
#include "edit_list.h"
#include "edit_data.h"
#include "preis_vergleich.h"

static void clear_screen(void) {
#if defined(_WIN32)
    system("cls");
#else
    // ANSI Clear: Bildschirm löschen + Cursor nach Home
    printf("\033[2J\033[H");
    fflush(stdout);
#endif
}

static void wait_enter(void) {
    printf("\nWeiter mit Enter ...");
    int c;
    // Alles bis zum Zeilenende weglesen (falls noch was im Puffer ist)
    while ((c = getchar()) != '\n' && c != EOF) { /* flush */ }
}

static int read_int(const char *prompt) {
    char buf[64];
    for (;;) {
        printf("%s", prompt);
        if (!fgets(buf, sizeof buf, stdin)) {
            clearerr(stdin);
            continue;
        }
        // \n entfernen
        buf[strcspn(buf, "\r\n")] = '\0';
        if (buf[0] == '\0') {
            // Leere Eingabe -> erneut fragen
            continue;
        }
        char *end = NULL;
        long v = strtol(buf, &end, 10);
        if (end && *end == '\0') {
            return (int)v;
        }
        printf("Ungueltige Eingabe. Bitte Zahl eingeben.\n");
    }
}

int main(void) {
    const char *filename = "einkaufsliste.txt";

    for (;;) {
        //clear_screen();
        printf("==================\n");
        printf("      Hauptmenü\n");
        printf("-------------------------------\n");
        printf("<0> Ende\n");
        printf("<1> Artikel und Preise editieren\n");
        printf("<2> Einkauf editieren\n");
        printf("<3> Einkaufszettel drucken\n");
        printf("<4> Preise vergleichen\n\n");
        int choice = read_int("Ihre Auswahl: ");

        switch (choice) {
            case 0:
                printf("Programm wird beendet.\n");
                return 0;

            case 1:
                edit_data_menu(DATA_DIRECTORY);
                break;

            case 2:
                edit_list(filename);
                break;

            case 3:
                druck_einkauf(filename);
                wait_enter();
                break;

            case 4:
                compare_prices_menu(DATA_DIRECTORY);
                wait_enter();
                break;

            default:
                printf("Ungueltige Auswahl.\n");
                wait_enter();
                break;
        }
    }
}


/*  Statusupdate:
 *      -> Liste lädt nicht ganz
 *      -> Menüstruktur wackelig
 */
