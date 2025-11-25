# EK – Einkaufslisten-Manager

Ein Projekt zur Verwaltung von Einkaufslisten und Datenbearbeitung – geschrieben in **C** mit optionaler **Web-Komponente**.

---

## Inhaltsverzeichnis

- [Über das Projekt](#über-das-projekt)
- [Funktionen](#funktionen)
- [Technologien](#technologien)
- [Installation](#installation)
- [Anwendung](#anwendung)
- [Projektstruktur](#projektstruktur)
- [Beitrag leisten](#beitrag-leisten)
- [Lizenz](#lizenz)

---

## Über das Projekt

**EK** ist ein C-basiertes System zur Verwaltung von Einkaufslisten.
Es erlaubt das Anlegen, Bearbeiten und Löschen von Einträgen sowie die Integration einer einfachen Weboberfläche.
Das Ziel ist es, den Einkaufsprozess zu digitalisieren und strukturierter zu gestalten.

---

## Funktionen

- Einkaufsliste im Browser verwalten – Artikel hinzufügen, bearbeiten, löschen
- CRUD-Datenoperationen – Create, Read, Update, Delete auf Artikeldaten
- Webserver – optionales Frontend im Browser --> Technologieschema: MVC-Controller

---

## Technologien

- **Sprache:** C  
- **Build-System:** CMake  
- **Web:** HTML, CSS, JavaScript (im Ordner `/web`)  

---

## Installation

-----------------------  IGNORE  -------------------------
/*
Windows:
1. [Für euch: Visual Studio 2022 Community oder Visual Studio Build Tools installieren (C++ Desktop-Workload)]
2. CMake (https://cmake.org) installieren und in PATH aufnehmen.
3. Projektordner öffnen, "cmake -S . -B build" ausführen, dann "cmake --build build"
4. Im Ordner build/bin "einkaufsprojekt.exe" starten und im Browser http://localhost:8081 öffnen.

Linux (Debian/Ubuntu): !!WICHTIG FÜR K**EK!!
1. Pakete installieren: sudo apt update && sudo apt install build-essential cmake.
2. Projektordner öffnen und "cmake -S . -B build" ausführen.
3. Mit "cmake --build build" kompilieren.
4. Aus build/bin ./einkaufsprojekt starten und im Browser http://localhost:8081 aufrufen
*/
----------------------------------------------------------

1. Repository klonen  
   ```bash
   git clone https://github.com/bigJannsen/EK.git
   cd EK

2. Build-VZ erstellen
   mkdir build
   cd build
   cmake ..

3. Kompilieren
   make

   (Optional) Daten vorbereiten

Erstelle eine Datei einkaufsliste.txt im Ordner /data oder passe den Pfad im Code an.
Starte bei Bedarf den Webserver:
./web_server

Öffne danach:
http://localhost:8081



Projektstruktur:


EK/
│  CMakeLists.txt
│  README.md
│  .gitignore
│
├─ include/
│   ├─ csv_database_utils.h
│   ├─ database_controller.h
│   ├─ database_core_defs.h
│   ├─ quantity_unit_utils.h
│   ├─ text_input_utils.h
│   ├─ web_server.h
│   └─ windows/
│       └─ dirent.h
│
├─ src/
│   ├─ csv_database_utils.c
│   ├─ database_controller.c
│   ├─ main.c
│   ├─ quantity_unit_utils.c
│   ├─ text_input_utils.c
│   └─ web_server.c
│
├─ data/
│   ├─ datenbanken.csv
│   └─ einkaufsliste.txt
│
├─ web/
│   ├─ index.html
│   ├─ styles.css
│   └─ script.js

-------------- TODO ----------------

### To-Do-Liste

- [ ] Ordnerstruktur verbessern (src/, include/, data/, web/)
- [ ] config.h für Port & Pfade erstellen
- [ ] Logging-System hinzufügen
- [ ] Einheitliches Error-Handling (enum error_t)
- [ ] JSON-Ausgabe über cJSON einbauen
- [ ] API-Funktionen (api_get_list, api_add_item, …)
- [ ] CORS-Header setzen
- [ ] Frontend-Fehlerbehandlung mit .catch
- [ ] Unit Tests (Unity oder cmocka)
- [ ] Speicherprüfung mit Valgrind
- [ ] Dockerfile anlegen
