# e3dcset - E3DC S10 Steuerung & Überwachung

Ein leistungsstarkes Linux-Kommandozeilen-Tool zur Steuerung und Überwachung von E3DC S10 Hauskraftwerken über das RSCP-Protokoll. Abfragen von Echtzeit-Werten, Batterie-Verwaltung und Analyse historischer Energiedaten.

## Funktionen

✨ **Leistungsmanagement**
- Batterie-Lade-/Entladeleistung einstellen
- Wechsel zum automatischen Leistungsmanagement
- Manuelle Batterie-Ladung mit spezifischer Energiemenge starten/stoppen
- Notstromreserve setzen (Workaround für Netzladung)

📊 **Daten-Abfragen**
- Echtzeit-Werte von beliebigen RSCP-Tags abfragen
- Nach Tag-Name oder Hex-Wert suchen
- Quiet-Mode für Skriptierung und Automatisierung
- **NEU:** JSON-Output-Modus (`-j`) für strukturierte Datenverarbeitung

📈 **Historische Datenanalyse**
- Abfrage aggregierter Energiedaten für Tag/Woche/Monat/Jahr
- Automatische Sampling-Optimierung pro Zeitraum
- Energiesummen mit Autarkie- und Eigenverbrauchsmetriken
- Flexible Datumsauswahl (YYYY-MM-DD oder 'today')

🏷️ **Dynamisches Tag-Management**
- Lade Tag-Definitionen aus externer Konfigurationsdatei
- Unterstützung für benutzerdefinierte Tag-Beschreibungen und Interpretationen
- Keine Neukompilierung für neue Tags erforderlich

## Schnelleinstieg

### Installation

Repository klonen:
```bash
git clone https://github.com/mschlappa/e3dcset.git
cd e3dcset
```

**Option 1: Config-Datei** (empfohlen für permanente Installation)

Deine E3DC-System-Zugangsdaten in `e3dcset.config` konfigurieren:
```bash
nano e3dcset.config
```

Diese Einstellungen aktualisieren:
```
server_ip=192.168.1.100
server_port=5033
e3dc_user=deine_email@beispiel.de
e3dc_password=dein_passwort
aes_password=dein_rscp_passwort
debug=0
```

**⚠️ Sicherheitshinweis:** Die Config-Datei enthält Klartext-Passwörter!
```bash
# Berechtigungen auf nur-Besitzer setzen:
chmod 600 e3dcset.config
```

**Option 2: Environment-Variablen** (empfohlen für Docker/CI)

**NEU:** Zugangsdaten können via Environment-Variablen übergeben werden (überschreiben Config-Datei):
```bash
export E3DC_USER="deine_email@beispiel.de"
export E3DC_PASSWORD="dein_passwort"
export E3DC_AES_PASSWORD="dein_rscp_passwort"

# Jetzt ohne Credentials in Config-Datei nutzbar:
./e3dcset -r EMS_BAT_SOC
```

Dies ist sicherer für:
- Docker-Container
- CI/CD-Pipelines
- Systeme mit Secret-Management

Tool kompilieren:
```bash
make
```

### Erste Schritte

Aktuellen Batterie-Status prüfen:
```bash
./e3dcset -r EMS_BAT_SOC
# Ausgabe: Tag 0x01000008: 85
```

Heutige Energieproduktion anzeigen:
```bash
./e3dcset -H day
# Ausgabe: Zeitraum: 21.11.2025 - 21.11.2025
#          PV-Produktion: 15.76 kWh
#          ...
```

## Nutzungsanleitung

### Leistungssteuerungs-Befehle

Batterie auf 2000W Laden stellen (Entladen blockieren):
```bash
./e3dcset -c 2000 -d 1
```

Batterie auf 2400W Laden und 500W Entladen stellen:
```bash
./e3dcset -c 2400 -d 500
```

Zurück zu automatischem Leistungsmanagement:
```bash
./e3dcset -a
```

Manuelles Laden mit 5kWh bei 2400W starten:
```bash
./e3dcset -c 2400 -d 1 -e 5000
```

Laufendes manuelles Laden stoppen:
```bash
./e3dcset -e 0
```

### Notstromreserve setzen (Workaround für Netzladung)

Da das manuelle Laden (`-e`) nur bedingt zuverlässig funktioniert, kann über die Notstromreserve eine Netzladung erzwungen werden. Das E3DC lädt die Batterie automatisch aus dem Netz, bis die Reserve erreicht ist.

Notstromreserve auf 3000 Wh setzen:
```bash
./e3dcset -E 3000
```

Notstromreserve deaktivieren:
```bash
./e3dcset -E 0
```

### Echtzeit-Werte abfragen

Batterie-Ladezustand (SOC) abfragen:
```bash
./e3dcset -r EMS_BAT_SOC
```

**Batterie-Gesundheitsüberwachung:**
```bash
# Relativer Ladezustand (Portal-Anzeige, 0-100%)
./e3dcset -r BAT_REQ_RSOC

# Absoluter SOC / State of Health (Batterie-Gesundheit, %)
./e3dcset -r BAT_REQ_ASOC

# Anzahl Ladezyklen
./e3dcset -r BAT_REQ_CHARGE_CYCLES

# Batterie-Strom und Spannung
./e3dcset -r BAT_REQ_CURRENT
./e3dcset -r BAT_REQ_MODULE_VOLTAGE
```

**Hinweis:** Die `BAT_REQ_*` Tags nutzen automatisch den BAT_REQ_DATA Container - das Tool kümmert sich um die korrekte Anfrage-Struktur.

**Multi-Batterie-Systeme:**

Wenn Ihr E3DC System mehrere Batterie-Module hat, können Sie mit dem `-i` Parameter das gewünschte Modul auswählen:

```bash
# Standard: Erstes Modul (Index 0)
./e3dcset -r BAT_REQ_RSOC              # Modul 0

# Zweites Modul abfragen (Index 1)
./e3dcset -r BAT_REQ_RSOC -i 1         # Modul 1

# Drittes Modul (Index 2)
./e3dcset -r BAT_REQ_ASOC -i 2         # Modul 2

# Alle Module im Script abfragen
for i in 0 1 2; do
  echo "Modul $i SOH: $(./e3dcset -r BAT_REQ_ASOC -i $i -q)%"
done
```

**Alle Werte eines Moduls auf einmal anzeigen (inkl. ALLE DCB-Zellblöcke):**

Mit der `-m` Option können Sie alle wichtigen Werte eines Batterie-Moduls **inklusive aller DCB-Module** in einer übersichtlichen Ausgabe erhalten:

```bash
# Alle Werte von Modul 0 (inkl. alle DCBs)
./e3dcset -m 0

# Ausgabe (Beispiel mit 2 DCB-Modulen):
Batterie Modul 0:
  Relativer SOC (Portal-Anzeige):  85.50 %
  Absoluter SOC / State of Health: 98.20 %
  Anzahl Ladezyklen:               234
  Batterie-Strom:                  -12.50 A
  Modulspannung:                   51.20 V
  Max. Batteriespannung:           58.80 V
  Batterie-Statuscode:             0
  Fehler-Code:                     0
  Anzahl DCB-Module:               2

  === DCB Zellblöcke ===
  Zellblock 0:
    State of Health (SOH):         48.90 %
    Ladezyklen:                    1774
    Strom:                         -2.50 A
    Spannung:                      51.20 V
    Volle Ladekapazität:           96.50 Ah
    Verbleibende Kapazität:        47.20 Ah
    ...

  Zellblock 1:
    State of Health (SOH):         84.90 %
    Ladezyklen:                    1557
    Strom:                         -2.48 A
    Spannung:                      51.18 V
    Volle Ladekapazität:           96.80 Ah
    Verbleibende Kapazität:        82.20 Ah
    ...
```

**NEU:** Das Tool fragt automatisch **alle DCB-Module** ab und zeigt deren detaillierte Informationen an:
- State of Health (SOH) pro DCB
- Ladezyklen pro DCB
- Strom/Spannung pro DCB
- Kapazitätsdaten pro DCB

Dies ermöglicht die **Überwachung der Gesundheit einzelner Batterie-Zellblöcke**, insbesondere nützlich bei:
- Systemen mit unterschiedlich alten DCB-Modulen
- Erkennung von degradierten Zellblöcken
- Präzise Batterie-Zustandsüberwachung

PV-Produktionsleistung abfragen:
```bash
./e3dcset -r EMS_POWER_PV
```

Mit direktem Hex-Wert abfragen:
```bash
./e3dcset -r 0x01000001
```

Mit Quiet-Mode abfragen (für Skripte):
```bash
SOC=$(./e3dcset -r EMS_BAT_SOC -q)
RSOC=$(./e3dcset -r BAT_REQ_RSOC -q)
ASOC=$(./e3dcset -r BAT_REQ_ASOC -q)
```

**NEU:** Mit JSON-Output abfragen (für strukturierte Datenverarbeitung):
```bash
./e3dcset -r EMS_BAT_SOC -j
# Ausgabe: {"tag":"EMS_BAT_SOC","hex":"0x01000008","value":85,"unit":"%"}

# JSON mit jq parsen:
POWER=$(./e3dcset -r EMS_POWER_PV -j | jq -r '.value')
echo "PV-Leistung: ${POWER}W"

# Historische Daten als JSON:
./e3dcset -H day -j
# Ausgabe: {"period":{"start":"2026-02-10","end":"2026-02-10"},"data":{"pv_production":4.64,...}}
```

### Verfügbare Tags durchsuchen

Alle Tag-Kategorien anzeigen:
```bash
./e3dcset -l
```

EMS (Energiemanagementsystem) Tags auflisten:
```bash
./e3dcset -l 1
```

Batterie-Tags auflisten:
```bash
./e3dcset -l 2
```

**Kategorien-Übersicht:**
| ID | Kategorie | Beschreibung |
|---|---|---|
| 1 | EMS | Energiemanagementsystem |
| 2 | BAT | Batterie |
| 3 | PVI | PV-Wechselrichter |
| 4 | PM | Leistungsmesser |
| 5 | WB | Wallbox / EV-Lader |
| 6 | DCDC | DC/DC-Wandler |
| 7 | INFO | Systeminformationen |
| 8 | DB | Datenbank (Geschichte) |

### Historische Datenabfragen

Heutige Energiesumme abfragen (24h, 15-Minuten-Intervalle):
```bash
./e3dcset -H day
```

Diese Woche Energiesumme abfragen (7 Tage, 1-Stunden-Intervalle):
```bash
./e3dcset -H week
```

Aktueller Monat Energiesumme abfragen (30 Tage, tägliche Intervalle):
```bash
./e3dcset -H month
```

Aktuelles Jahr Energiesumme abfragen (365 Tage, wöchentliche Intervalle):
```bash
./e3dcset -H year
```

Historische Daten von einem bestimmten Datum abfragen:
```bash
./e3dcset -H day -D 2024-11-20
./e3dcset -H week -D 2024-11-15
./e3dcset -H month -D 2024-10-01
./e3dcset -H year -D 2023-01-01
```

**Ausgabe-Beispiel:**
```
Zeitraum: 17.11.2025 - 23.11.2025
PV-Produktion:      43.92 kWh
Batterie geladen:   21.86 kWh
Batterie entladen:  15.35 kWh
Netzbezug:          126.65 kWh
Netzeinspeisung:    6.17 kWh
Hausverbrauch:      152.91 kWh
Autarkie:           17.2 %
```

## Befehlsreferenz

```
Verwendung: e3dcset [-c Ladeleistung] [-d Entladeleistung] [-e Energiemenge] [-a] 
                    [-r TAG_NAME] [-q] [-l [kategorie]] [-H typ] [-D datum]
                    [-p config_pfad] [-t tags_pfad]

Leistungssteuerung:
  -c <watt>     Ladeleistung einstellen (Watt)
  -d <watt>     Entladeleistung einstellen (Watt, 1 = deaktiviert)
  -e <wh>       Manuelles Laden mit Energiemenge starten (Wh, 0 = stoppen)
  -E <wh>       Notstromreserve setzen (Wh, 0 = deaktivieren)
  -a            Zurück zu automatischem Leistungsmanagement

Daten-Abfragen:
  -r <tag>      RSCP-Tag-Wert abfragen (Name oder Hex wie 0x01000001)
  -i <index>    Batterie-Modul Index für BAT_REQ_* Tags (Standard: 0)
  -m <index>    Alle Werte eines Batterie-Moduls anzeigen
  -q            Quiet-Mode - nur Wert ausgeben (für Skriptierung)
  -j            JSON-Output - strukturierte Ausgabe (kombinierbar mit -r/-m/-H)
  -l [kat]      Tags nach Kategorie auflisten (1-8, kein Argument = Übersicht)

Historische Daten:
  -H <typ>      Historische Daten abfragen (day|week|month|year)
  -D <datum>    Datum angeben: YYYY-MM-DD oder 'today' (Standard: today)

Konfiguration:
  -p <pfad>     Benutzerdefinierten Config-Pfad angeben (Standard: e3dcset.config)
  -t <pfad>     Benutzerdefinierten Tags-Pfad angeben (Standard: e3dcset.tags)
```

### Wichtige Einschränkungen

- `-r` kann nicht mit `-c`, `-d`, `-e`, `-E`, `-a` oder `-H` kombiniert werden
- `-H` kann nicht mit `-r`, `-c`, `-d`, `-e`, `-E` oder `-a` kombiniert werden
- `-q` kann nur mit `-r` verwendet werden
- `-D` kann nur mit `-H` verwendet werden
- Nur REQUEST-Tags können abgefragt werden (zweites Byte < 0x80)

## Tag-Management

### Format der Tag-Definitionsdatei

Erstelle benutzerdefinierte Tag-Definitionen in `e3dcset.tags`:

```
# Kommentare beginnen mit #

[EMS]
TAG_NAME = 0xHEXVALUE # Beschreibung

[BAT]
TAG_NAME = 0xHEXVALUE # Beschreibung

[INTERPRETATIONS]
0xHEXVALUE:WERT = Interpretationstext
```

### Beispielkonfiguration

```
[EMS]
EMS_POWER_PV = 0x01000001 # PV-Leistung in Watt
EMS_BAT_SOC = 0x01000008 # Batterie-Ladezustand in Prozent

[INTERPRETATIONS]
0x01000011:0 = Normal/Automatik
0x01000011:1 = Leerlauf
0x01000011:2 = Entladung
0x01000011:3 = Ladung
0x01000011:4 = Netzladung
```

Benutzerdefinierte Tags-Datei verwenden:
```bash
./e3dcset -l -t /pfad/zu/custom.tags
./e3dcset -r EMS_POWER_PV -t /pfad/zu/custom.tags
```

## Skriptierungsbeispiele

### Batterie-Überwachung und automatisches Laden

```bash
#!/bin/bash
SOC=$(./e3dcset -r EMS_BAT_SOC -q)
echo "Batterie: ${SOC}%"

if [ $(echo "$SOC < 30" | bc) -eq 1 ]; then
    echo "Lade mit 3000W..."
    ./e3dcset -c 3000 -d 1
fi
```

### Täglicher Energiebericht

```bash
#!/bin/bash
echo "=== Täglicher Energiebericht ==="
./e3dcset -H day
```

### Wöchentliche Analyse

```bash
#!/bin/bash
echo "=== Wöchentliche Zusammenfassung ==="
./e3dcset -H week

echo ""
echo "=== Monatliche Zusammenfassung ==="
./e3dcset -H month
```

### System-Gesundheitsprüfung

```bash
#!/bin/bash
echo "EMS-Modus:"
./e3dcset -r EMS_MODE

echo ""
echo "PV-Status:"
./e3dcset -r PVI_ON_GRID

echo ""
echo "Batterie:"
./e3dcset -r EMS_BAT_SOC -q | xargs -I {} echo "SOC: {}%"
```

## Fehlerbehebung

### Verbindungsprobleme

**Fehler: "Connection refused"**
- Verifiziere IP-Adresse und Port in `e3dcset.config`
- Prüfe Netzwerkverbindung zum E3DC-System
- Stelle sicher, dass die Firewall TCP 5033 erlaubt

### Tag-Fehler

**Fehler: "RESPONSE Tag"**
- Nur REQUEST-Tags können abgefragt werden (zweites Byte < 0x80)
- Gültiger Bereich: `0x01xxxxxx` bis `0x08xxxxxx`
- Verwende `-l` um gültige Tags zu finden

**Fehler: "Tag not found"**
- Verifiziere Tag-Name oder Hex-Wert
- Prüfe, dass Tags-Datei existiert und geladen wird
- Verwende `-l 1` bis `-l 8` um verfügbare Tags zu durchsuchen

### Historische Daten

**Keine historischen Daten zurückgegeben:**
- Verifiziere, dass das E3DC-System historische Daten hat
- Prüfe Datumsformat: muss YYYY-MM-DD sein
- Stelle sicher, dass das Datum im verfügbaren Datenbereich liegt
- Die meisten Systeme haben Daten von mehreren Wochen/Monaten zurück

## Build & Development

### Kompilierung

**Einfacher Build:**
```bash
make          # Kompiliert e3dcset
```

**Clean Build:**
```bash
make clean    # Entfernt alle Build-Artefakte
make          # Neu kompilieren
```

**Mit Tests:**
```bash
make test     # Kompiliert und führt alle Tests aus
```

### Modulare Code-Struktur

**NEU:** Der Code ist modular aufgeteilt für bessere Wartbarkeit:

```
e3dcset/
├── e3dcset.cpp              # Main-Funktion + Argument-Parsing
├── config.cpp/h             # Config-Parsing, Validierung, Credentials
├── rscp_handler.cpp/h       # RSCP Request/Response Handling
├── output.cpp/h             # Output-Formatierung (Text + JSON)
├── history.cpp/h            # History-Daten & Timestamp-Handling
├── RscpProtocol.cpp/.h      # RSCP-Protokoll-Implementierung
├── SocketConnection.cpp/.h  # Netzwerkkommunikation mit Timeouts
├── AES.cpp/.h               # AES-256-Verschlüsselung
├── RscpTags.h               # Protokoll-Tag-Konstanten
├── RscpTypes.h              # Protokoll-Datentyp-Definitionen
├── Makefile                 # Build mit Auto-Dependency-Tracking
├── tests/                   # Unit-Tests (C)
│   ├── test_config.c
│   ├── test_safe_string.c
│   └── test_validation.c
└── README.md                # Diese Datei
```

### Automatisches Dependency-Tracking

**NEU:** Das Makefile nutzt automatische Dependency-Generierung:
- Änderungen an Header-Files triggern automatisch Rebuild der betroffenen Dateien
- Kein manuelles `make clean` mehr nötig nach Header-Änderungen
- `.d` Files werden automatisch während Kompilierung generiert

### Tests

Das Projekt enthält Unit-Tests für kritische Funktionen:

```bash
make test     # Alle Tests ausführen
```

**Getestete Module:**
- Config-Parsing (Integer, String, Validierung)
- String-Handling (safe_string_copy, Bounds-Checking)
- Input-Validation (Datum, Leistung, Ladungsmenge)

**Test-Output:**
```
=========================================
Running e3dcset Test Suite
=========================================

=== Testing Config Parsing ===
  Running: config_parse_integer... PASS
  ...
  Total:  9 | Passed: 9 | Failed: 0

All tests passed! ✓
```

### Verbesserte Fehlerbehandlung

**NEU:** Robuste Error-Handling-Features:

1. **Config-Validierung:**
   - Prüft Pflichtfelder (server_ip, credentials) nach dem Laden
   - Klare Fehlermeldungen bei fehlenden Werten
   - Hinweise auf Environment-Variablen

2. **Null-Pointer-Checks:**
   - Safe-Strdup mit Allocation-Check
   - LocalTime_r Null-Check bei Timestamp-Konvertierung
   - Clear Error-Messages bei Fehlern

3. **DCB-Loop-Protection:**
   - Max 3 Retries bei fehlenden DCB-Daten
   - Verhindert Endlosschleifen bei Kommunikationsproblemen
   - Graceful Exit mit Fehlermeldung

4. **Socket-Timeouts:**
   - 10s Receive-Timeout (erhöht von 3s)
   - Verhindert unbegrenztes Blockieren
   - Besser für History-Queries mit großen Datenmengen

5. **Timestamp-Validierung:**
   - Range-Check (1970-2100) für Timestamps
   - Fallback bei ungültigen Werten
   - Warnung bei Out-of-Range-Daten

## Projektstruktur

Siehe [Build & Development](#build--development) für Details zur modularen Code-Struktur.

## Technische Details

### RSCP-Protokoll

Die Kommunikation mit E3DC-Systemen verwendet das RSCP (RES Charge Protocol):
- Verschlüsselte TCP-Verbindung auf Port 5033
- AES-256-Verschlüsselung für Datensicherheit
- Request/Response-Container-Architektur
- Automatische Geräte-Authentifizierung

### Datensammlung Verlauf

Das System sammelt aggregierte Energiedaten in verschiedenen Intervallen:

| Zeitraum | Dauer | Intervall | Granularität |
|----------|-------|-----------|--------------|
| Tag | 24 Stunden | 15 min | Stündliche Details verfügbar |
| Woche | 7 Tage | 1 Stunde | Tägliche Summaries |
| Monat | 30 Tage | 1 Tag | Wöchentliche Muster |
| Jahr | 365 Tage | 1 Woche | Saisonale Trends |

Alle Werte werden in Gesamtenergie (kWh) plus Effizienzkennzahlen (Autarkie %, Eigenverbrauch %) aggregiert.

## Kompatibilität

- **Zielsystem**: E3DC S10
- **Protokoll**: RSCP (RES Charge Protocol kompatibel)
- **OS**: Linux (Raspberry Pi, Debian, Ubuntu, etc.)
- **Compiler**: g++ (C++11 oder später)

## Lizenz

Dieses Projekt respektiert das geistige Eigentum von E3DC. Für kommerzielle Nutzung oder Vertrieb konsultiere E3DC.

## Danksagungen

Basiert auf der E3DC RSCP-Protokoll-Dokumentation und Beispielen. Weitere Entwicklung inspiriert durch Community-Beiträge.

## Unterstützung

Bei Fragen oder Problemen:
1. Prüfe den [Fehlerbehebung](#fehlerbehebung)-Bereich
2. Verifiziere deine Konfigurationsdatei-Einstellungen
3. Überprüfe verfügbare Tags mit `-l`
4. Aktiviere Debug-Mode in der Konfigurationsdatei für detaillierte Logs

---

**Version**: 2.2  
**Zuletzt aktualisiert**: 10.02.2026  
**Betreut**: Community-gesteuert

**Changelog v2.2:**
- ✅ Environment-Variablen-Support für Credentials
- ✅ JSON-Output-Modus (`-j`)
- ✅ Modulare Code-Struktur (config, rscp_handler, output, history)
- ✅ Automatisches Dependency-Tracking im Build-System
- ✅ Unit-Tests für Config-Parsing und Validierung
- ✅ Verbesserte Fehlerbehandlung (Config-Validierung, Null-Checks, DCB-Retry-Limit)
- ✅ Socket-Timeout erhöht (10s) für stabilere History-Queries
- ✅ Timestamp-Validierung für History-Daten
