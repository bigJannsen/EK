# EK – Einkaufslisten-Manager

Ein schlanker Einkaufsmanager in **C11** mit integriertem **HTTP-Server**. Die Anwendung verwaltet CSV-basierte Artikeldatenbanken, führt Preisvergleiche durch und speichert Einkaufslisten, die im Browser über das mitgelieferte Web-Frontend bedient werden können.

---

## Inhaltsverzeichnis
- [Überblick](#überblick)
- [Hauptfunktionen](#hauptfunktionen)
- [Voraussetzungen](#voraussetzungen)
- [Build & Start](#build--start)
- [API-Überblick](#api-überblick)
- [Projektstruktur](#projektstruktur)
- [Datenablage](#datenablage)
- [Weiterentwicklung](#weiterentwicklung)

---

## Überblick
- Eigenständiger Server (`run_server`) mit standardmäßigem Port **8081** (konfiguriert in `config.h`).
- Statische Assets aus `web/` werden unter `/` ausgeliefert; die JSON-API läuft unter `/api/*`.
- Pfade für Daten (`data/`) und Web-Inhalte werden über CMake beim Build als Defines gesetzt, sodass keine manuelle Konfiguration zur Laufzeit nötig ist.

---

## Hauptfunktionen
- **CSV-Datenbanken verwalten**
  - Listet verfügbare CSV-Dateien in `data/` und lädt sie als Datenbank.
  - CRUD-Operationen für Artikel (Name, Anbieter, Preis in Cent, Menge/Wert + Einheit) mit Validierung und automatischer ID-Vergabe.
- **Einkaufslisten bearbeiten**
  - Liest und schreibt `data/einkaufsliste.txt`.
  - Einträge können hinzugefügt, geändert, gelöscht oder als Download bereitgestellt werden.
- **Preisvergleich**
  - Vergleicht zwei Datenbankeinträge hinsichtlich Einheitspreis und Gesamtbetrag für eine Menge.
  - Analysiert eine komplette Einkaufsliste, schlägt günstige Anbieter vor und kann die Liste optional automatisch anpassen.
- **Web-Frontend**
  - Browser-Oberfläche mit Tabellen für Artikel und Einkaufsliste sowie Formularen für CRUD-Operationen.
  - Nutzt die API-Endpunkte und zeigt Preisempfehlungen direkt an.

---

## Voraussetzungen
- C-Compiler mit **C11**-Unterstützung (z. B. GCC, Clang oder MSVC).
- **CMake ≥ 3.10**.
- POSIX-Sockets oder Winsock (wird automatisch unter Windows gelinkt).

---

## Build & Start
1. Repository klonen
   ```bash
   git clone https://github.com/bigJannsen/EK.git
   cd EK
   ```
2. Build-Verzeichnis anlegen & konfigurieren
   ```bash
   cmake -S . -B build
   ```
3. Kompilieren
   ```bash
   cmake --build build
   ```
4. Server starten
   ```bash
   ./build/bin/einkaufsprojekt
   ```
5. Browser öffnen: [http://localhost:8081](http://localhost:8081)

Hinweis: Beim Start werden die aktiven Konfigurationswerte (Port, Log-Level, Limits) auf der Konsole ausgegeben.

---

## API-Überblick
Die wichtigsten Endpunkte (alle liefern JSON):

- **Datenbanken**
  - `GET /api/db-files` – verfügbare CSV-Dateien auflisten.
  - `GET /api/db?name=<datei>` – gesamte Datenbank lesen.
  - `POST /api/db/add|update|delete` – Einträge anlegen, ändern oder löschen.
- **Einkaufsliste**
  - `GET /api/list` – aktuelle Liste lesen.
  - `POST /api/list/add|update|delete` – Einträge verwalten.
  - `GET /api/list/download` – Liste als Text herunterladen.
- **Vergleich**
  - `POST /api/compare/single` – zwei Artikel anhand einer Menge vergleichen.
  - `POST /api/compare/list` – komplette Liste optimieren (optional auto-apply).
- **Statische Dateien**
  - `GET /` oder `GET /static/<pfad>` – Web-Frontend laden.

---

## Projektstruktur
```
EK/
├─ CMakeLists.txt           # Build-Definition, Pfade/Defines für Daten & Web
├─ include/                 # Öffentliche Header für Config, Datenbank, Webserver
├─ src/                     # Implementierungen (Server, API-Handler, Datenbank)
├─ data/                    # Beispiel-Datenbanken & Einkaufsliste
└─ web/                     # Statische Assets (HTML, CSS, JS)
```

---

## Datenablage
- Standard-Datenverzeichnis: `data/`
- Beispiel-Datenbank: `data/datenbanken.csv` (Artikel mit Anbieter, Preis, Mengeninfo)
- Einkaufsliste: `data/einkaufsliste.txt` (eine Zeile pro Artikel/Anbieter)

---

## Weiterentwicklung
- Logging-Verbesserungen und erweiterte Fehlerbehandlung.
- Zusätzliche Tests (Unit- und Integrationstests) für API- und Datenbanklogik.
- Optional: Docker-Setup für reproduzierbare Deployments.
