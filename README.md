# e3dcset - E3DC S10 Steuerung & Überwachung

![Build](https://github.com/jarvis-schlappa/e3dcset/actions/workflows/build.yml/badge.svg)

Ein leistungsstarkes Kommandozeilen-Tool zur Steuerung und Überwachung von E3DC S10 Hauskraftwerken über das RSCP-Protokoll.

**Funktionen:**
- ⚡ Batterie-Lade-/Entladeleistung steuern
- 📊 Echtzeit-Werte von RSCP-Tags abfragen (inkl. JSON-Output)
- 📈 Historische Energiedaten analysieren (Tag/Woche/Monat/Jahr)
- 🔋 Batterie-Gesundheit überwachen (SOC, SOH, Ladezyklen)
- 🏷️ Dynamisches Tag-Management über externe Konfigurationsdatei

---

## Schnellstart

### Installation

```bash
# Repository klonen
git clone https://github.com/jarvis-schlappa/e3dcset.git
cd e3dcset

# Konfiguration erstellen (siehe e3dcset.config.example)
cp e3dcset.config.example e3dcset.config
nano e3dcset.config
chmod 600 e3dcset.config

# Kompilieren
make
```

### Erste Schritte

```bash
# Batterie-Ladezustand prüfen
./e3dcset -r EMS_BAT_SOC

# Heutige PV-Produktion anzeigen
./e3dcset -H day

# Batterie auf 2000W laden, Entladung blockieren
./e3dcset -c 2000 -d 1

# Zurück zu automatischem Modus
./e3dcset -a
```

### JSON-Output für Automatisierung

```bash
# Strukturierte Daten ausgeben (NDJSON – ein JSON-Objekt pro Zeile)
./e3dcset -r EMS_BAT_SOC -j
# Output: {"tag":"EMS_BAT_SOC","hex":"0x01000008","value":85,"unit":"%"}

# System-Informationen als JSON
./e3dcset --info -j

# Mit jq verarbeiten
POWER=$(./e3dcset -r EMS_POWER_PV -j | jq -r '.value')
echo "PV-Leistung: ${POWER}W"
```

### System-Informationen

```bash
# SW-Version, Seriennummer, Produktionsdatum
./e3dcset --info
```

### History-Rohdaten (CSV)

```bash
# 15-Minuten-Einzelwerte als CSV
./e3dcset -H day --raw
./e3dcset -H day -D 2024-11-20 --raw
```

---

## Dokumentation

📖 **Vollständige Dokumentation:**
- **[USAGE.md](docs/USAGE.md)** – CLI-Referenz, alle Flags & Beispiele
- **[CONFIGURATION.md](docs/CONFIGURATION.md)** – Config-Format, Umgebungsvariablen
- **[BUILDING.md](docs/BUILDING.md)** – Build-Anleitung, Tests, Plattformen
- **[ARCHITECTURE.md](docs/ARCHITECTURE.md)** – Code-Struktur, Module
- **[CHANGELOG.md](docs/CHANGELOG.md)** – Versionshistorie

---

## Befehlsübersicht

```
Leistungssteuerung:
  -c <watt>     Ladeleistung setzen
  -d <watt>     Entladeleistung setzen (1 = blockiert)
  -e <wh>       Manuelles Laden mit Energiemenge (0 = stoppen)
  -E <wh>       Notstromreserve setzen (0 = deaktivieren)
  -a            Automatisches Leistungsmanagement

Datenabfragen:
  -r <tag>      RSCP-Tag abfragen (Name oder Hex)
  -b <index>    Device-Index für BAT/PVI Container-Tags (Standard: 0)
  -m <index>    Alle Werte eines Batterie-Moduls inkl. DCBs
  -q            Quiet-Mode (nur Wert ausgeben)
  -j            JSON/NDJSON-Output
  --info        System-Informationen (SW-Version, Seriennummer, Produktionsdatum)
  -l [kat]      Tags auflisten (1-8)
  -w            Watch-Mode (kontinuierliche Überwachung)
  --interval    Watch-Intervall in Sekunden (Standard: 5)

Historische Daten:
  -H <typ>      History abfragen (day|week|month|year)
  -D <datum>    Datum angeben (YYYY-MM-DD, Standard: today)
  --raw         CSV-Ausgabe der History-Einzelwerte (nur mit -H)

Allgemein:
  -h, --help    Hilfe anzeigen
  -p <pfad>     Config-Pfad (Standard: e3dcset.config)
  -t <pfad>     Tags-Pfad (Standard: e3dcset.tags)
```

Siehe [USAGE.md](docs/USAGE.md) für Details und Beispiele.

---

## Beispiele

**Batterie-Gesundheit überwachen:**
```bash
./e3dcset -r BAT_REQ_RSOC         # Relativer SOC (Portal-Anzeige)
./e3dcset -r BAT_REQ_ASOC         # State of Health
./e3dcset -r BAT_REQ_CHARGE_CYCLES # Ladezyklen
./e3dcset -m 0                     # Alle Werte inkl. DCB-Module
```

**Historische Daten:**
```bash
./e3dcset -H day                   # Heute
./e3dcset -H week -D 2024-11-15    # Bestimmte Woche
./e3dcset -H month -j              # Monat als JSON
```

**Skriptierung:**
```bash
SOC=$(./e3dcset -r EMS_BAT_SOC -q)
if [ $SOC -lt 30 ]; then
  ./e3dcset -c 3000 -d 1  # Batterie laden
fi
```

---

## Kompatibilität

- **System:** E3DC S10 Hauskraftwerke
- **Protokoll:** RSCP (AES-256-verschlüsselt)
- **OS:** Linux (Debian, Ubuntu, Raspberry Pi), macOS
- **Compiler:** g++ (C++11 oder neuer)

---

## Support & Entwicklung

- **Tests:** `make test`
- **Build:** `make clean && make`
- **Debug:** `debug=1` in Config-Datei setzen

Bei Problemen siehe [USAGE.md – Fehlerbehebung](docs/USAGE.md#fehlerbehebung).

---

**Version:** 2.2  
**Lizenz:** Siehe [LICENSE](LICENSE)  
**Maintainer:** Community-driven

**Basiert auf:** E3DC RSCP-Protokoll-Dokumentation
