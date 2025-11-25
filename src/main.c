#include <stdio.h>
#include "web_server.h"

// Alen
//Startet den Serverprozess für die Anwendung
//Ruft die HTTP Startfunktion auf ohne weitere Logik
int main(void) {
    return run_server();
}




/* Antwort: Das Programm startet in main nur den run_server() aus web_server.c. web_server.c
 * verarbeitet alle HTTP-Anfragen selbst und nutzt dafür direkt die Funktionen aus
 * crud_database.c; die Module edit_data.c, edit_list.c und preis_vergleich.c werden auf
 * diesem Weg nicht eingebunden. */

/*
Windows:
1. [Für euch: Visual Studio 2022 Community oder Visual Studio Build Tools installieren (C++ Desktop-Workload)]
2. CMake (https://cmake.org) installieren und in PATH aufnehmen.
3. Projektordner öffnen, "cmake -S . -B build" ausführen, dann "cmake --build build"
4. Im Ordner build/bin "einkaufsprojekt.exe" starten und im Browser http://localhost:8081 öffnen.

Linux (Debian/Ubuntu): !!WICHTIG FÜR KU**EK!!
1. Pakete installieren: sudo apt update && sudo apt install build-essential cmake.
2. Projektordner öffnen und "cmake -S . -B build" ausführen.
3. Mit "cmake --build build" kompilieren.
4. Aus build/bin ./einkaufsprojekt starten und im Browser http://localhost:8081 aufrufen
*/
