# e3dcset - E3DC S10 Steuerung & Überwachung

Ein leistungsstarkes Linux-Kommandozeilen-Tool zur Steuerung und Überwachung von E3DC S10 Hauskraftwerken über das RSCP-Protokoll. Abfragen von Echtzeit-Werten, Batterie-Verwaltung und Analyse historischer Energiedaten.

## Funktionen

✨ **Leistungsmanagement**
- Batterie-Lade-/Entladeleistung einstellen
- Wechsel zum automatischen Leistungsmanagement
- Manuelle Batterie-Ladung mit spezifischer Energiemenge starten/stoppen

📊 **Daten-Abfragen**
- Echtzeit-Werte von beliebigen RSCP-Tags abfragen
- Nach Tag-Name oder Hex-Wert suchen
- Quiet-Mode für Skriptierung und Automatisierung

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

### Echtzeit-Werte abfragen

Batterie-Ladezustand (SOC) abfragen:
```bash
./e3dcset -r EMS_BAT_SOC
```

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
  -a            Zurück zu automatischem Leistungsmanagement

Daten-Abfragen:
  -r <tag>      RSCP-Tag-Wert abfragen (Name oder Hex wie 0x01000001)
  -q            Quiet-Mode - nur Wert ausgeben (für Skriptierung)
  -l [kat]      Tags nach Kategorie auflisten (1-8, kein Argument = Übersicht)

Historische Daten:
  -H <typ>      Historische Daten abfragen (day|week|month|year)
  -D <datum>    Datum angeben: YYYY-MM-DD oder 'today' (Standard: today)

Konfiguration:
  -p <pfad>     Benutzerdefinierten Config-Pfad angeben (Standard: e3dcset.config)
  -t <pfad>     Benutzerdefinierten Tags-Pfad angeben (Standard: e3dcset.tags)
```

### Wichtige Einschränkungen

- `-r` kann nicht mit `-c`, `-d`, `-e`, `-a` oder `-H` kombiniert werden
- `-H` kann nicht mit `-r`, `-c`, `-d`, `-e` oder `-a` kombiniert werden
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

## Projektstruktur

```
e3dcset/
├── e3dcset.cpp              # Hauptprogramm & CLI
├── e3dcset.config           # Konfiguration (Zugangsdaten, Limits)
├── e3dcset.tags             # Tag-Definitionen & Interpretationen
├── RscpProtocol.cpp/.h      # RSCP-Protokoll-Implementierung
├── SocketConnection.cpp/.h  # Netzwerkkommunikation
├── AES.cpp/.h               # AES-256-Verschlüsselung
├── RscpTags.h               # Protokoll-Tag-Konstanten
├── RscpTypes.h              # Protokoll-Datentyp-Definitionen
├── Makefile                 # Build-Konfiguration
└── README.md                # Diese Datei
```

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

**Version**: 2.0  
**Zuletzt aktualisiert**: 21.11.2025  
**Betreut**: Community-gesteuert
