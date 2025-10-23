#include <stdio.h>
#include "web_server.h"

int main(void) {
    return run_server();
}

/*
Windows:
1. Visual Studio 2022 Community oder Visual Studio Build Tools installieren (C++ Desktop-Workload).
2. CMake (https://cmake.org) installieren und in PATH aufnehmen.
3. Projektordner öffnen, "cmake -S . -B build" ausführen, danach "cmake --build build".
4. Im Ordner build/bin "einkaufsprojekt.exe" starten und im Browser http://localhost:8080 öffnen.

Linux (Debian/Ubuntu):
1. Pakete installieren: sudo apt update && sudo apt install build-essential cmake.
2. Projektordner öffnen und "cmake -S . -B build" ausführen.
3. Mit "cmake --build build" kompilieren.
4. Aus build/bin ./einkaufsprojekt starten und im Browser http://localhost:8080 aufrufen.
*/
