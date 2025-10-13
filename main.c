#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "druck_einkauf.h"
#include "edit_list.h"
#include "edit_data.h"

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
        printf("==========================\n");
        printf("   Einkaufsprojekt v1.0\n");
        printf("==========================\n");
        printf("[1] Einkaufsliste anzeigen\n");
        printf("[2] Liste bearbeiten\n");
        printf("[3] CSV-Datenbanken bearbeiten\n");
        printf("[4] Beenden\n");
        printf("==========================\n");

        int choice = read_int("Auswahl: ");

        switch (choice) {
            case 1:
                //clear_screen();
                druck_einkauf(filename);
                wait_enter();
                break;

            case 2:
                //clear_screen();
                edit_list(filename);   // kehrt nach Speichern/Zurueck zurueck
                break;

            case 3:
                edit_data_menu(DATA_DIRECTORY);
                break;

            case 4:
                printf("Programm wird beendet.\n");
                return 0;

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
