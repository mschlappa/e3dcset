# USAGE – Vollständige CLI-Referenz

Komplette Anleitung zur Nutzung von `e3dcset` mit allen Flags, Optionen und Beispielen.

---

## Inhaltsverzeichnis

- [Grundlegende Syntax](#grundlegende-syntax)
- [Leistungssteuerung](#leistungssteuerung)
- [Datenabfragen](#datenabfragen)
- [Historische Daten](#historische-daten)
- [Watch-Mode](#watch-mode)
- [Tag-Management](#tag-management)
- [Ausgabeformate](#ausgabeformate)
- [Skriptierung](#skriptierung)
- [Fehlerbehebung](#fehlerbehebung)

---

## Grundlegende Syntax

```bash
e3dcset [OPTIONEN]
```

**Wichtige Einschränkungen:**
- `-r` (Abfrage) kann **nicht** mit Steuerungsbefehlen (`-c`, `-d`, `-e`, `-E`, `-a`) kombiniert werden
- `-H` (Historie) kann **nicht** mit anderen Aktionen kombiniert werden
- `-q` (Quiet) funktioniert nur mit `-r`
- `-j` (JSON) funktioniert mit `-r`, `-m`, `-H`
- `-D` (Datum) funktioniert nur mit `-H`

---

## Leistungssteuerung

### Batterie Ladeleistung setzen (`-c`)

```bash
e3dcset -c <watt>
```

Setzt die Batterie-Ladeleistung in Watt.

**Beispiele:**
```bash
# Batterie mit 2000W laden
./e3dcset -c 2000

# Batterie mit maximaler Leistung laden (z.B. 4600W)
./e3dcset -c 4600
```

**Hinweis:** Wird oft mit `-d 1` kombiniert, um Entladung zu blockieren.

---

### Batterie Entladeleistung setzen (`-d`)

```bash
e3dcset -d <watt>
```

Setzt die Batterie-Entladeleistung in Watt.

**Spezialwert:** `-d 1` = Entladung **blockiert**

**Beispiele:**
```bash
# Entladung blockieren
./e3dcset -d 1

# Entladung auf 500W begrenzen
./e3dcset -d 500

# Batterie auf 2400W laden, 500W Entladung erlauben
./e3dcset -c 2400 -d 500
```

---

### Manuelles Laden (`-e`)

```bash
e3dcset -e <wh>
```

Startet manuelles Laden mit definierter Energiemenge in Wattstunden (Wh).

**Spezialwert:** `-e 0` = Manuelles Laden **stoppen**

**Beispiele:**
```bash
# 5 kWh mit 2400W laden (Entladung blockiert)
./e3dcset -c 2400 -d 1 -e 5000

# Laufendes manuelles Laden stoppen
./e3dcset -e 0
```

**⚠️ Hinweis:** Manuelles Laden funktioniert nicht immer zuverlässig. Siehe `-E` als Alternative.

---

### Notstromreserve setzen (`-E`)

```bash
e3dcset -E <wh>
```

Setzt die Notstromreserve in Wattstunden (Wh). Das E3DC-System lädt automatisch aus dem Netz, bis die Reserve erreicht ist.

**Spezialwert:** `-E 0` = Reserve **deaktivieren**

**Beispiele:**
```bash
# Notstromreserve auf 3000 Wh setzen
./e3dcset -E 3000

# Netzladung erzwingen: Reserve auf 80% setzen
./e3dcset -E 8000  # Bei 10 kWh Batterie

# Notstromreserve deaktivieren
./e3dcset -E 0
```

**💡 Tipp:** Nutze `-E` anstelle von `-e` für zuverlässigeres Laden aus dem Netz.

---

### Automatisches Leistungsmanagement (`-a`)

```bash
e3dcset -a
```

Wechselt zurück zum automatischen Leistungsmanagement des E3DC-Systems.

**Beispiel:**
```bash
# Nach manueller Steuerung zurück zu automatisch
./e3dcset -a
```

---

## Datenabfragen

### RSCP-Tag abfragen (`-r`)

```bash
e3dcset -r <tag>
```

Fragt einen RSCP-Tag ab. `<tag>` kann ein **Tag-Name** oder **Hex-Wert** sein.

**Beispiele:**
```bash
# Batterie-Ladezustand (Name)
./e3dcset -r EMS_BAT_SOC
# Output: Tag 0x01000008: 85

# Batterie-Ladezustand (Hex)
./e3dcset -r 0x01000008
# Output: Tag 0x01000008: 85

# PV-Leistung
./e3dcset -r EMS_POWER_PV
# Output: Tag 0x01000001: 3420

# Hausverbrauch
./e3dcset -r EMS_POWER_HOME
# Output: Tag 0x0100000C: 1850
```

**Wichtig:** Nur **REQUEST-Tags** können abgefragt werden (zweites Byte < 0x80).  
Beispiel: `0x01000008` ✅ (REQUEST), `0x01800008` ❌ (RESPONSE)

---

### Batterie-Gesundheit überwachen

**Spezielle Batterie-Tags:**
```bash
# Relativer SOC (Portal-Anzeige, 0-100%)
./e3dcset -r BAT_REQ_RSOC

# Absoluter SOC / State of Health (Batterie-Gesundheit, %)
./e3dcset -r BAT_REQ_ASOC

# Anzahl Ladezyklen
./e3dcset -r BAT_REQ_CHARGE_CYCLES

# Batterie-Strom (Ampere)
./e3dcset -r BAT_REQ_CURRENT

# Modulspannung (Volt)
./e3dcset -r BAT_REQ_MODULE_VOLTAGE
```

**Hinweis:** `BAT_REQ_*` Tags nutzen automatisch den `BAT_REQ_DATA` Container.

---

### Multi-Batterie-Systeme (`-i`)

```bash
e3dcset -r <tag> -i <index>
```

Bei Systemen mit mehreren Batterie-Modulen kann mit `-i` das Modul ausgewählt werden.

**Beispiele:**
```bash
# Erstes Modul (Index 0, Standard)
./e3dcset -r BAT_REQ_RSOC

# Zweites Modul (Index 1)
./e3dcset -r BAT_REQ_RSOC -i 1

# Drittes Modul (Index 2)
./e3dcset -r BAT_REQ_ASOC -i 2

# Alle Module in Script abfragen
for i in 0 1 2; do
  echo "Modul $i SOH: $(./e3dcset -r BAT_REQ_ASOC -i $i -q)%"
done
```

---

### Modul-Info Dump (`-m`)

```bash
e3dcset -m <index>
```

Zeigt **alle Werte** eines Batterie-Moduls inklusive **aller DCB-Zellblöcke** an.

**Beispiel:**
```bash
./e3dcset -m 0
```

**Ausgabe:**
```
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

**Nutzen:**
- Überwachung einzelner Zellblöcke
- Erkennung degradierter DCB-Module
- Präzise Batterie-Zustandsüberwachung

---

## Historische Daten

### Historie abfragen (`-H`)

```bash
e3dcset -H <typ>
```

Fragt historische Energiedaten ab. `<typ>` kann sein:
- `day` – 24 Stunden (15-Minuten-Intervalle)
- `week` – 7 Tage (1-Stunden-Intervalle)
- `month` – 30 Tage (Tages-Intervalle)
- `year` – 365 Tage (Wochen-Intervalle)

**Beispiele:**
```bash
# Heute
./e3dcset -H day

# Diese Woche
./e3dcset -H week

# Dieser Monat
./e3dcset -H month

# Dieses Jahr
./e3dcset -H year
```

**Ausgabe:**
```
Zeitraum: 10.02.2026 - 10.02.2026
PV-Produktion:      15.76 kWh
Batterie geladen:   8.24 kWh
Batterie entladen:  6.12 kWh
Netzbezug:          3.45 kWh
Netzeinspeisung:    2.18 kWh
Hausverbrauch:      17.23 kWh
Autarkie:           79.8 %
Eigenverbrauch:     86.2 %
```

---

### Datum angeben (`-D`)

```bash
e3dcset -H <typ> -D <datum>
```

Fragt historische Daten für ein bestimmtes Datum ab.

**Format:** `YYYY-MM-DD` oder `today`

**Beispiele:**
```bash
# Bestimmter Tag
./e3dcset -H day -D 2024-11-20

# Bestimmte Woche (Woche die den 15.11. enthält)
./e3dcset -H week -D 2024-11-15

# Bestimmter Monat (Monat Oktober 2024)
./e3dcset -H month -D 2024-10-01

# Bestimmtes Jahr (2023)
./e3dcset -H year -D 2023-01-01

# Heute (explizit)
./e3dcset -H day -D today
```

---

## Watch-Mode

### Kontinuierliche Überwachung (`-w`)

```bash
e3dcset -w -r <tag>
```

Fragt einen Tag kontinuierlich in festem Intervall ab. **Strg+C** zum Beenden.

**Beispiele:**
```bash
# Batterie-SOC alle 5 Sekunden (Standard)
./e3dcset -w -r EMS_BAT_SOC

# PV-Leistung alle 10 Sekunden
./e3dcset -w --interval 10 -r EMS_POWER_PV

# JSON-Output für Live-Logging
./e3dcset -w -r EMS_POWER_HOME -j >> power.log
```

**Ausgabe:**
```
2026-02-10 22:15:03  Tag 0x01000008: 85
2026-02-10 22:15:08  Tag 0x01000008: 84
2026-02-10 22:15:13  Tag 0x01000008: 84
^C
```

---

### Watch-Intervall (`--interval`)

```bash
e3dcset -w --interval <sekunden> -r <tag>
```

Setzt das Intervall für Watch-Mode in Sekunden (Minimum: 1 Sekunde).

**Beispiele:**
```bash
# Alle 1 Sekunde (schnellst möglich)
./e3dcset -w --interval 1 -r EMS_POWER_PV

# Alle 60 Sekunden
./e3dcset -w --interval 60 -r EMS_BAT_SOC
```

---

## Tag-Management

### Tags auflisten (`-l`)

```bash
e3dcset -l [kategorie]
```

Listet verfügbare RSCP-Tags auf.

**Ohne Kategorie:** Zeigt Übersicht aller Kategorien

**Mit Kategorie:** Listet alle Tags der Kategorie

**Kategorien:**
| ID | Name | Beschreibung |
|----|------|--------------|
| 1 | EMS | Energiemanagementsystem |
| 2 | BAT | Batterie |
| 3 | PVI | PV-Wechselrichter |
| 4 | PM | Leistungsmesser |
| 5 | WB | Wallbox / EV-Lader |
| 6 | DCDC | DC/DC-Wandler |
| 7 | INFO | Systeminformationen |
| 8 | DB | Datenbank (Historie) |

**Beispiele:**
```bash
# Übersicht
./e3dcset -l

# EMS-Tags auflisten
./e3dcset -l 1

# Batterie-Tags auflisten
./e3dcset -l 2

# Mit custom Tags-Datei
./e3dcset -l 1 -t /pfad/zu/custom.tags
```

---

### Custom Tags-Datei (`-t`)

```bash
e3dcset -t <pfad>
```

Lädt benutzerdefinierte Tag-Definitionen aus einer Datei.

**Format:** Siehe [CONFIGURATION.md – Tags-Datei](CONFIGURATION.md#tags-datei-format)

**Beispiel:**
```bash
# Custom Tags verwenden
./e3dcset -r MY_CUSTOM_TAG -t ~/my-tags.txt
```

---

## Ausgabeformate

### Normal (Standard)

```bash
./e3dcset -r EMS_BAT_SOC
```

**Ausgabe:**
```
Tag 0x01000008: 85
```

---

### Quiet-Mode (`-q`)

Gibt **nur den Wert** aus, ohne Formatierung. Ideal für Skripte.

```bash
./e3dcset -r EMS_BAT_SOC -q
```

**Ausgabe:**
```
85
```

**Verwendung:**
```bash
SOC=$(./e3dcset -r EMS_BAT_SOC -q)
echo "Batterie: ${SOC}%"
```

---

### JSON-Output (`-j`)

Gibt strukturierte JSON-Daten aus. Kombinierbar mit `-r`, `-m`, `-H`.

**Abfrage:**
```bash
./e3dcset -r EMS_BAT_SOC -j
```

**Ausgabe:**
```json
{"tag":"EMS_BAT_SOC","hex":"0x01000008","value":85,"unit":"%"}
```

**Historie:**
```bash
./e3dcset -H day -j
```

**Ausgabe:**
```json
{
  "period": {
    "start": "2026-02-10",
    "end": "2026-02-10"
  },
  "data": {
    "pv_production": 15.76,
    "battery_charge": 8.24,
    "battery_discharge": 6.12,
    "grid_import": 3.45,
    "grid_export": 2.18,
    "home_consumption": 17.23,
    "autarky": 79.8,
    "self_consumption": 86.2
  },
  "unit": "kWh"
}
```

**Mit jq verarbeiten:**
```bash
# Einzelwert extrahieren
POWER=$(./e3dcset -r EMS_POWER_PV -j | jq -r '.value')

# Historie-Daten extrahieren
AUTARKY=$(./e3dcset -H day -j | jq -r '.data.autarky')
echo "Autarkie heute: ${AUTARKY}%"
```

---

## Skriptierung

### Batterie-Überwachung

```bash
#!/bin/bash
SOC=$(./e3dcset -r EMS_BAT_SOC -q)
echo "Batterie: ${SOC}%"

if [ $SOC -lt 30 ]; then
    echo "Batterie niedrig! Lade mit 3000W..."
    ./e3dcset -c 3000 -d 1
elif [ $SOC -gt 95 ]; then
    echo "Batterie voll! Zurück zu Auto-Modus..."
    ./e3dcset -a
fi
```

---

### Täglicher Energiebericht

```bash
#!/bin/bash
echo "=== Energiebericht $(date +%Y-%m-%d) ==="
./e3dcset -H day

# Als JSON speichern
./e3dcset -H day -j > ~/energy-reports/$(date +%Y-%m-%d).json
```

---

### System-Gesundheitsprüfung

```bash
#!/bin/bash
echo "=== E3DC System-Check ==="
echo "Batterie SOC: $(./e3dcset -r EMS_BAT_SOC -q)%"
echo "Batterie SOH: $(./e3dcset -r BAT_REQ_ASOC -q)%"
echo "Ladezyklen:   $(./e3dcset -r BAT_REQ_CHARGE_CYCLES -q)"
echo "PV-Leistung:  $(./e3dcset -r EMS_POWER_PV -q)W"
echo "Netz:         $(./e3dcset -r EMS_POWER_GRID -q)W"
```

---

### Live-Monitoring mit Watch + JSON

```bash
#!/bin/bash
# Live-Daten in JSON-Format loggen
./e3dcset -w --interval 10 -r EMS_POWER_PV -j | \
while read line; do
  timestamp=$(date +%s)
  echo "{\"timestamp\":$timestamp,$line:1:-1}}"
done >> ~/logs/pv-power.jsonl
```

---

## Fehlerbehebung

### Verbindungsprobleme

**Fehler:** `Connection refused`

**Lösung:**
1. IP-Adresse und Port in `e3dcset.config` prüfen
2. Netzwerk-Verbindung zum E3DC-System testen:
   ```bash
   ping <E3DC_IP>
   telnet <E3DC_IP> 5033
   ```
3. Firewall-Regeln prüfen (TCP Port 5033 erlauben)

---

### Authentication-Fehler

**Fehler:** `Authentication failed` oder `AES error`

**Lösung:**
1. Credentials in Config-Datei überprüfen:
   - `e3dc_user` = Portal-Email
   - `e3dc_password` = Portal-Passwort
   - `aes_password` = RSCP-Passwort (meist anders!)
2. Passwörter mit Sonderzeichen in Quotes setzen
3. Environment-Variablen prüfen (überschreiben Config):
   ```bash
   env | grep E3DC
   ```

---

### Tag-Fehler

**Fehler:** `RESPONSE Tag`

**Erklärung:** Nur REQUEST-Tags können abgefragt werden (zweites Byte < 0x80).

**Lösung:**
- ✅ Gültig: `0x01000008` (REQUEST)
- ❌ Ungültig: `0x01800008` (RESPONSE)

Nutze `-l` um gültige Tags zu finden:
```bash
./e3dcset -l 1  # EMS-Tags
```

---

**Fehler:** `Tag not found`

**Lösung:**
1. Tag-Name überprüfen (case-sensitive!)
2. Tags-Datei prüfen:
   ```bash
   ./e3dcset -l 1
   ```
3. Custom Tags-Datei verwenden:
   ```bash
   ./e3dcset -r MY_TAG -t ~/my-tags.txt
   ```

---

### Historische Daten

**Fehler:** Keine Daten zurückgegeben

**Lösung:**
1. Datumsformat prüfen: `YYYY-MM-DD`
2. Datum im verfügbaren Bereich? (meist letzte Wochen/Monate)
3. Debug-Modus aktivieren:
   ```bash
   # In e3dcset.config
   debug=1
   ```

**Hinweis:** E3DC-Systeme speichern typischerweise Daten für mehrere Monate.

---

### Socket-Timeout

**Fehler:** `Timeout waiting for response`

**Lösung:**
- Normale Abfragen sollten < 3s dauern
- Historische Daten (besonders `year`) können bis zu 10s brauchen
- Bei wiederholten Timeouts: E3DC-System neu starten

---

### Weitere Hilfe

Bei ungelösten Problemen:
1. **Debug-Modus aktivieren:**
   ```bash
   # In e3dcset.config
   debug=1
   ```
2. **Verfügbare Tags prüfen:**
   ```bash
   ./e3dcset -l
   ```
3. **Build-Tests ausführen:**
   ```bash
   make test
   ```

---

**Zurück zu:** [README](../README.md) | **Siehe auch:** [CONFIGURATION.md](CONFIGURATION.md), [BUILDING.md](BUILDING.md)
