# CHANGELOG – Versionshistorie

Alle bemerkenswerten Änderungen an `e3dcset` werden in dieser Datei dokumentiert.

Format basiert auf [Keep a Changelog](https://keepachangelog.com/de/1.0.0/).

---

## [Unreleased]

### Dokumentation
- **Neue Dokumentationsstruktur:**
  - Kompakte README.md (1-Seiten-Übersicht)
  - docs/USAGE.md (vollständige CLI-Referenz)
  - docs/CONFIGURATION.md (Config-Format, Env-Vars)
  - docs/BUILDING.md (Build-Anleitung, Plattformen)
  - docs/ARCHITECTURE.md (Code-Struktur, Design)
  - docs/CHANGELOG.md (diese Datei)
- `e3dcset.config.example` mit allen Feldern und Kommentaren

---

## [2.2] – 2026-02-10

### Neue Features

#### Environment-Variablen-Support
- **Credentials via Environment-Variablen** übertragbar:
  - `E3DC_USER` – Portal-Benutzername
  - `E3DC_PASSWORD` – Portal-Passwort
  - `E3DC_AES_PASSWORD` – RSCP-Passwort
  - `E3DC_SERVER_IP` – E3DC IP-Adresse
  - `E3DC_SERVER_PORT` – RSCP Port
- Environment-Variablen **überschreiben** Config-Datei-Werte
- Nützlich für Docker, CI/CD, Kubernetes Secrets

#### JSON-Output-Modus
- **`-j` / `--json` Flag** für strukturierte Datenausgabe
- Funktioniert mit:
  - Tag-Abfragen (`-r`)
  - Modul-Dump (`-m`)
  - Historie-Abfragen (`-H`)
- **Format:**
  ```json
  {"tag":"EMS_BAT_SOC","hex":"0x01000008","value":85,"unit":"%"}
  ```
- Ideal für Skriptierung mit `jq`

#### Watch-Mode
- **`-w` / `--watch` Flag** für kontinuierliche Überwachung
- **`--interval <sekunden>` Option** zum Setzen des Intervalls (Standard: 5s)
- Strg+C zum sauberen Beenden
- **Beispiel:**
  ```bash
  ./e3dcset -w --interval 10 -r EMS_BAT_SOC
  ```

---

### Code-Verbesserungen

#### Modulare Code-Struktur
- **Aufgeteilt in separate Module:**
  - `config.cpp/.h` – Config-Parsing, Validierung
  - `rscp_handler.cpp/.h` – RSCP Request/Response
  - `output.cpp/.h` – Output-Formatierung (Text, JSON)
  - `history.cpp/.h` – Historie-Daten, Timestamps
- Bessere Wartbarkeit und Testbarkeit
- Klare Verantwortlichkeiten

#### Automatisches Dependency-Tracking
- **Makefile generiert `.d` Dependency-Files**
- Header-Änderungen triggern automatisch Rebuild betroffener Dateien
- Kein manuelles `make clean` mehr nötig
- Best-Practice für C/C++-Projekte

#### Unit-Tests
- **Test-Framework implementiert** (`tests/test_framework.h`)
- **Tests für kritische Funktionen:**
  - Config-Parsing (Integer, String, Validierung)
  - String-Handling (`safe_strdup`, Bounds-Checking)
  - Input-Validation (Datum, Leistung, Energie)
- **`make test` Target** zum Ausführen aller Tests
- CI-ready (GitHub Actions)

---

### Fehlerbehandlung & Stabilität

#### Verbesserte Config-Validierung
- **Pflichtfelder werden geprüft** nach Laden:
  - `server_ip`, `e3dc_user`, `e3dc_password`, `aes_password`
- **Klare Fehlermeldungen** bei fehlenden Werten
- **Hinweise auf Environment-Variablen** als Alternative
- Exit mit sinnvollem Error-Code

#### Null-Pointer-Checks
- **`safe_strdup()` mit Allocation-Check**
  - Prüft `malloc()` Rückgabewert
  - Exit mit Fehlermeldung bei OOM
- **`localtime_r` Null-Check** bei Timestamp-Konvertierung
- Robuster gegen ungültige Eingaben

#### DCB-Loop-Protection
- **Maximale 3 Retries** beim Abfragen von DCB-Daten
- Verhindert Endlosschleifen bei Kommunikationsproblemen
- Graceful Exit mit Fehlermeldung

#### Socket-Timeout erhöht
- **Receive-Timeout: 3s → 10s**
- Verhindert Timeouts bei History-Queries (große Datenmengen)
- Besonders wichtig für `year`-Abfragen
- Konfigurierbar via `timeout_seconds` in Config

#### Timestamp-Validierung
- **Range-Check für Timestamps** (1970-2100)
- Fallback bei ungültigen Werten
- Warnung bei Out-of-Range-Daten
- Verhindert Abstürze durch korrupte History-Daten

---

### Plattform-Support

#### macOS-Kompatibilität
- **Header-Fixes:**
  - `malloc.h` → `stdlib.h` (via `#ifdef __APPLE__`)
  - Timestamp-Handling angepasst für macOS
- **Erfolgreich getestet auf:**
  - Apple Silicon (M1/M2)
  - Intel-based Macs
- Build "out of the box" ohne Patches

#### Linux-Optimierungen
- Verbesserte Socket-Handling
- Timeout-Konfiguration flexibler

---

### Dokumentation

- Umfangreiche Inline-Kommentare
- Verbesserte `README.md` mit Beispielen
- Hinweise auf Environment-Variablen
- Fehlerbehebungs-Sektion erweitert

---

## [2.1] – 2025-11-21

### Neue Features

#### Batterie-Gesundheitsüberwachung
- **`BAT_REQ_*` Tags** für detaillierte Batterie-Info:
  - `BAT_REQ_RSOC` – Relativer SOC (Portal-Anzeige, 0-100%)
  - `BAT_REQ_ASOC` – State of Health (Batterie-Gesundheit, %)
  - `BAT_REQ_CHARGE_CYCLES` – Anzahl Ladezyklen
  - `BAT_REQ_CURRENT` – Batterie-Strom (Ampere)
  - `BAT_REQ_MODULE_VOLTAGE` – Modulspannung (Volt)
- Automatisches `BAT_REQ_DATA` Container-Handling

#### Multi-Batterie-Support
- **`-i <index>` Flag** zum Auswählen von Batterie-Modulen
- Unterstützt Systeme mit mehreren Batterie-Modulen
- **Beispiel:**
  ```bash
  ./e3dcset -r BAT_REQ_RSOC -i 1  # Zweites Modul
  ```

#### Modul-Info Dump
- **`-m <index>` Flag** zeigt **alle Werte** eines Batterie-Moduls
- **Inkl. ALLE DCB-Zellblöcke:**
  - State of Health (SOH) pro DCB
  - Ladezyklen pro DCB
  - Strom/Spannung pro DCB
  - Kapazitätsdaten pro DCB
- Ermöglicht Überwachung einzelner Zellblöcke
- Nützlich für Systeme mit unterschiedlich alten DCB-Modulen

#### Notstromreserve setzen
- **`-E <wh>` Flag** zum Setzen der Notstromreserve
- Workaround für Netzladung (zuverlässiger als `-e`)
- E3DC lädt automatisch aus Netz bis Reserve erreicht
- **Beispiel:**
  ```bash
  ./e3dcset -E 3000  # 3000 Wh Reserve
  ./e3dcset -E 0     # Reserve deaktivieren
  ```

---

### Verbesserungen

- Erweiterte Tag-Definitionen in `e3dcset.tags`
- Bessere Fehlerausgaben bei ungültigen Tags
- Dokumentation für Batterie-Features erweitert

---

## [2.0] – 2025-11-15

### Neue Features

#### Historische Datenanalyse
- **`-H <typ>` Flag** für aggregierte Energiedaten:
  - `day` – 24 Stunden (15-Minuten-Intervalle)
  - `week` – 7 Tage (1-Stunden-Intervalle)
  - `month` – 30 Tage (Tages-Intervalle)
  - `year` – 365 Tage (Wochen-Intervalle)
- **`-D <datum>` Flag** zum Angeben eines Datums (YYYY-MM-DD oder 'today')
- **Ausgabe:**
  - PV-Produktion (kWh)
  - Batterie geladen/entladen (kWh)
  - Netzbezug/Einspeisung (kWh)
  - Hausverbrauch (kWh)
  - Autarkie (%)
  - Eigenverbrauch (%)

#### Dynamisches Tag-Management
- **Tag-Definitionen aus externer Datei** (`e3dcset.tags`)
- **`-t <pfad>` Flag** für custom Tags-Datei
- **`-l [kategorie]` Flag** zum Auflisten verfügbarer Tags
- **Kategorien:**
  - 1 = EMS (Energiemanagementsystem)
  - 2 = BAT (Batterie)
  - 3 = PVI (PV-Wechselrichter)
  - 4 = PM (Leistungsmesser)
  - 5 = WB (Wallbox)
  - 6 = DCDC (DC/DC-Wandler)
  - 7 = INFO (Systeminformationen)
  - 8 = DB (Datenbank / Historie)
- **Interpretationen:** Numerische Werte können als Text interpretiert werden
- **Keine Neukompilierung** für neue Tags nötig

#### Quiet-Mode
- **`-q` Flag** gibt nur den **Wert** aus (ohne Formatierung)
- Ideal für Skriptierung und Automatisierung
- **Beispiel:**
  ```bash
  SOC=$(./e3dcset -r EMS_BAT_SOC -q)
  echo "Batterie: ${SOC}%"
  ```

---

### Verbesserungen

- REQUEST/RESPONSE-Tag-Validierung (nur REQUEST-Tags erlaubt)
- Automatische Sampling-Optimierung für History-Queries
- Verbesserte Fehlerbehandlung bei ungültigen Datums-Formaten

---

## [1.0] – 2024-10-01

### Initiales Release

#### Leistungsmanagement
- **`-c <watt>` Flag** – Batterie-Ladeleistung setzen
- **`-d <watt>` Flag** – Batterie-Entladeleistung setzen (1 = blockiert)
- **`-e <wh>` Flag** – Manuelles Laden mit Energiemenge (0 = stoppen)
- **`-a` Flag** – Automatisches Leistungsmanagement aktivieren

#### Datenabfragen
- **`-r <tag>` Flag** – RSCP-Tag-Wert abfragen
  - Unterstützt Tag-Namen (z.B. `EMS_BAT_SOC`)
  - Unterstützt Hex-Werte (z.B. `0x01000008`)
- **Basis-Tags:** EMS_POWER_PV, EMS_BAT_SOC, EMS_MODE, etc.

#### Konfiguration
- **`-p <pfad>` Flag** – Custom Config-Pfad
- **Config-Format:**
  - `server_ip` – E3DC IP-Adresse
  - `server_port` – RSCP Port (Standard: 5033)
  - `e3dc_user` – Portal-Benutzername
  - `e3dc_password` – Portal-Passwort
  - `aes_password` – RSCP-Passwort
  - `debug` – Debug-Modus (0/1)

#### Sicherheit
- **AES-256-Verschlüsselung** für RSCP-Kommunikation
- **TCP-Socket-Verbindung** zum E3DC-System
- Authentifizierung via Portal-Credentials

---

## Geplant für nächste Versionen

### [2.3] – Geplant
- [ ] MQTT-Support für Home Assistant Integration
- [ ] Prometheus-Exporter für Monitoring
- [ ] Wallbox-Steuerung (`WB_*` Tags)
- [ ] Auto-Discovery von E3DC-Systemen im Netzwerk
- [ ] Config-Wizard für erste Einrichtung

### [3.0] – Roadmap
- [ ] Web-Interface (optional)
- [ ] Historische Daten in SQLite speichern
- [ ] Grafische Darstellung (gnuplot Integration)
- [ ] Multi-System-Support (mehrere E3DC-Anlagen parallel)

---

## Changelog-Konventionen

### Kategorien
- **Added** – Neue Features
- **Changed** – Änderungen an existierenden Features
- **Deprecated** – Features die in Zukunft entfernt werden
- **Removed** – Entfernte Features
- **Fixed** – Bug-Fixes
- **Security** – Sicherheits-Fixes

### Versionsnummern
Format: `MAJOR.MINOR.PATCH`
- **MAJOR** – Breaking Changes (API-Änderungen)
- **MINOR** – Neue Features (abwärtskompatibel)
- **PATCH** – Bug-Fixes (abwärtskompatibel)

---

**Zurück zu:** [README](../README.md) | **Siehe auch:** [USAGE.md](USAGE.md), [CONFIGURATION.md](CONFIGURATION.md)
