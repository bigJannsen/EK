# 🚀 EK – Einkaufslisten-Manager

Ein schlanker Einkaufsmanager in **C11** mit integriertem **HTTP-Server**.  
Die Anwendung verwaltet CSV-basierte Artikeldatenbanken, vergleicht Preise und speichert Einkaufslisten, die komfortabel über das mitgelieferten Web-Frontend im Browser bedient werden können.

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

- Eigenständiger HTTP-Server (`run_server`) auf Port **8081** (konfigurierbar in `config.h`).
- Pfade für Daten (`data/`) und Web-Inhalte (`web/`) werden über CMake als Defines gesetzt – keine manuelle Konfiguration nötig.
- Komplett eigenständig: keine externen Bibliotheken, kein Framework, kein Interpreter.

---

## Hauptfunktionen

### 🔹 CSV-Datenbanken
- Listet CSV-Dateien in `data/` und lädt sie als Datenbank.
- CRUD-Operationen für Artikel (Name, Anbieter, Preis in Cent, Menge/Einheit) inkl.:
   - Validierung aller Eingaben
   - automatischer ID-Vergabe
   - einheitlicher Mengen- und Preisverarbeitung

### 🔹 Einkaufslisten
- Liest und schreibt `data/einkaufsliste.txt`.
- Unterstützung für:
   - Eintrag hinzufügen
   - Eintrag bearbeiten
   - Eintrag löschen
   - Liste als Text herunterladen

### 🔹 Preisvergleich
- Vergleich zweier Artikel hinsichtlich:
   - Einheitspreis
   - Gesamtpreis für eine Zielmenge
- Analyse kompletter Einkaufslisten:
   - schlägt günstigste Anbieter vor
   - optional automatische Anpassung (Auto-Apply)

### 🔹 Web-Frontend
- Browser-Oberfläche mit:
   - Tabellenansicht für Datenbank & Einkaufsliste
   - CRUD-Formulare
   - Direktanzeige von Preisempfehlungen
- Alle Aktionen laufen über den integrierten Webserver.

---

## Voraussetzungen

- C-Compiler mit **C11**-Unterstützung (GCC, Clang oder MSVC).
- **CMake ≥ 3.10**.
- POSIX-Sockets oder Winsock (unter Windows automatisch aktiviert).

---

## Build & Start

1. Repository klonen:
   ```bash
   git clone https://github.com/bigJannsen/EK.git
   cd EK
   
   optional bei Linux: sudo apt install cmake--build-debug
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

Alle Endpunkte liefern JSON zurück.

### 🔹 Datenbanken
| Methode | Pfad                       | Beschreibung                       |
|--------|-----------------------------|------------------------------------|
| GET    | `/api/db-files`            | Verfügbare CSV-Dateien auflisten   |
| GET    | `/api/db?name=<datei>`     | gesamte Datenbank lesen            |
| POST   | `/api/db/add`              | neuen Eintrag anlegen              |
| POST   | `/api/db/update`           | Eintrag aktualisieren              |
| POST   | `/api/db/delete`           | Eintrag löschen                    |

### 🔹 Einkaufsliste
| Methode | Pfad                       | Beschreibung                   |
|--------|-----------------------------|--------------------------------|
| GET    | `/api/list`                | Liste auslesen                 |
| POST   | `/api/list/add`            | Eintrag hinzufügen             |
| POST   | `/api/list/update`         | Eintrag bearbeiten             |
| POST   | `/api/list/delete`         | Eintrag entfernen              |
| GET    | `/api/list/download`       | Liste als Text herunterladen   |

### 🔹 Preisvergleich
| Methode | Pfad                       | Beschreibung                        |
|--------|-----------------------------|-------------------------------------|
| POST   | `/api/compare/single`      | zwei Artikel vergleichen            |
| POST   | `/api/compare/list`        | komplette Einkaufsliste optimieren  |

### 🔹 Statische Dateien
- `GET /`
- `GET /static/...`

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


---

## Datenablage

- Standard-Datenverzeichnis: `data/`
- Beispiel-Datenbank: `data/datenbanken.csv`
- Einkaufsliste: `data/einkaufsliste.txt`

---

## Weiterentwicklung

- Frontend-Verbesserungen (optional Framework)
- Verbesserte Logging- und Fehlerbehandlung
- Unit- und Integrationstests für API & Datenbank
- Optional: Docker-Setup für reproduzierbare Deployments

---

## Lizenz

Dieses Projekt steht unter der **GNU General Public License v3 (GPLv3)**.

Die Nutzung, Veränderung und Weitergabe des Codes ist erlaubt, solange alle abgeleiteten Werke ebenfalls unter der GPLv3 veröffentlicht werden.

Der vollständige Lizenztext befindet sich in der Datei:  
[`LICENSE`](LICENSE)

---