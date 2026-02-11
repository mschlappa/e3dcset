# CONFIGURATION – Konfiguration & Einstellungen

Vollständige Anleitung zur Konfiguration von `e3dcset` über Config-Dateien und Umgebungsvariablen.

---

## Inhaltsverzeichnis

- [Config-Datei Format](#config-datei-format)
- [Umgebungsvariablen](#umgebungsvariablen)
- [Tags-Datei Format](#tags-datei-format)
- [Sicherheit](#sicherheit)
- [Erweiterte Einstellungen](#erweiterte-einstellungen)

---

## Config-Datei Format

### Standard-Pfad

**Default:** `e3dcset.config` (im aktuellen Verzeichnis)

**Custom-Pfad:**
```bash
./e3dcset -p /pfad/zu/config.conf -r EMS_BAT_SOC
```

---

### Basis-Konfiguration

**Minimale Config (Pflichtfelder):**
```ini
server_ip=192.168.1.100
server_port=5033
e3dc_user=deine_email@beispiel.de
e3dc_password=dein_passwort
aes_password=dein_rscp_passwort
```

**Vollständige Config mit allen Optionen:**
```ini
# === Verbindungseinstellungen ===
server_ip=192.168.1.100        # IP-Adresse des E3DC-Systems
server_port=5033               # RSCP-Port (Standard: 5033)

# === Authentifizierung ===
e3dc_user=deine_email@beispiel.de     # E3DC-Portal Benutzername (Email)
e3dc_password=dein_portal_passwort    # E3DC-Portal Passwort
aes_password=dein_rscp_passwort       # RSCP-Passwort (meist unterschiedlich!)

# === Erweiterte Einstellungen ===
timeout_seconds=10             # Socket-Timeout in Sekunden (Standard: 10)
max_retries=3                  # Maximale Anzahl Verbindungsversuche (Standard: 3)
debug=0                        # Debug-Modus: 0=aus, 1=ein (Standard: 0)
```

---

### Parameter-Beschreibung

#### `server_ip` *(Pflicht)*

IP-Adresse des E3DC S10 Hauskraftwerks im lokalen Netzwerk.

**Beispiel:**
```ini
server_ip=192.168.1.100
```

**Finden der IP:**
- E3DC-Portal → System → Netzwerk
- Router-Webinterface (DHCP-Leases)
- `nmap 192.168.1.0/24 -p 5033`

---

#### `server_port` *(Pflicht)*

TCP-Port für RSCP-Kommunikation.

**Standard:** `5033`

```ini
server_port=5033
```

**Hinweis:** Sollte nicht geändert werden, außer bei spezieller Firewall-Konfiguration.

---

#### `e3dc_user` *(Pflicht)*

Benutzername (Email-Adresse) für das E3DC-Portal.

```ini
e3dc_user=max.mustermann@beispiel.de
```

**Wichtig:** Muss mit dem Portal-Account übereinstimmen!

---

#### `e3dc_password` *(Pflicht)*

Passwort für das E3DC-Portal.

```ini
e3dc_password=MeinSicheresPasswort123
```

**Hinweis:** Bei Sonderzeichen in Quotes setzen:
```ini
e3dc_password="Pass!wort#mit$Zeichen"
```

---

#### `aes_password` *(Pflicht)*

RSCP-Passwort für AES-256-Verschlüsselung der Kommunikation.

```ini
aes_password=RscpPasswort456
```

**⚠️ Wichtig:** Das RSCP-Passwort ist **meist unterschiedlich** vom Portal-Passwort!

**Finden des RSCP-Passworts:**
- E3DC-Portal → Einstellungen → Personalisierung → RSCP-Passwort
- Erstinstallation: Wird bei E3DC-System-Einrichtung gesetzt

---

#### `timeout_seconds` *(Optional)*

Socket-Timeout für Netzwerk-Kommunikation in Sekunden.

**Standard:** `10`

```ini
timeout_seconds=10
```

**Empfehlungen:**
- Normale Abfragen: `3-5` Sekunden
- Historische Daten: `10-15` Sekunden
- Langsame Netzwerke: `20` Sekunden

---

#### `max_retries` *(Optional)*

Maximale Anzahl Verbindungsversuche bei Fehler.

**Standard:** `3`

```ini
max_retries=3
```

**Hinweis:** Bei instabilen Netzwerken höher setzen (z.B. `5`).

---

#### `debug` *(Optional)*

Debug-Modus für detaillierte Logging-Ausgaben.

**Werte:**
- `0` = Debug aus (Standard)
- `1` = Debug an

```ini
debug=1
```

**Debug-Ausgabe:**
```
[DEBUG] Verbinde zu 192.168.1.100:5033...
[DEBUG] AES-Verschlüsselung initialisiert
[DEBUG] Sende REQUEST: TAG=0x01000008
[DEBUG] Empfange RESPONSE: Länge=64 Bytes
[DEBUG] Tag 0x01000008: 85
```

**Nutzen:**
- Verbindungsprobleme debuggen
- RSCP-Protokoll-Kommunikation nachvollziehen
- Performance-Probleme identifizieren

---

## Umgebungsvariablen

### Übersicht

Umgebungsvariablen **überschreiben** Werte aus der Config-Datei. Nützlich für:
- Docker-Container
- CI/CD-Pipelines
- Systeme mit Secret-Management (z.B. Kubernetes Secrets)

---

### Verfügbare Variablen

| Variable | Config-Parameter | Beschreibung |
|----------|------------------|--------------|
| `E3DC_USER` | `e3dc_user` | Portal-Benutzername |
| `E3DC_PASSWORD` | `e3dc_password` | Portal-Passwort |
| `E3DC_AES_PASSWORD` | `aes_password` | RSCP-Passwort |
| `E3DC_SERVER_IP` | `server_ip` | E3DC IP-Adresse |
| `E3DC_SERVER_PORT` | `server_port` | RSCP Port |

---

### Verwendung

**Export in Shell:**
```bash
export E3DC_USER="max.mustermann@beispiel.de"
export E3DC_PASSWORD="MeinPasswort123"
export E3DC_AES_PASSWORD="RscpPasswort456"
export E3DC_SERVER_IP="192.168.1.100"
export E3DC_SERVER_PORT="5033"

# Jetzt ohne Credentials in Config nutzbar:
./e3dcset -r EMS_BAT_SOC
```

---

**Inline (einmalig):**
```bash
E3DC_USER="user@beispiel.de" \
E3DC_PASSWORD="pass" \
E3DC_AES_PASSWORD="aes" \
./e3dcset -r EMS_BAT_SOC
```

---

**Docker:**
```dockerfile
ENV E3DC_USER="user@beispiel.de"
ENV E3DC_PASSWORD="pass"
ENV E3DC_AES_PASSWORD="aes"
ENV E3DC_SERVER_IP="192.168.1.100"
```

Oder mit Docker Secrets:
```bash
docker run \
  -e E3DC_USER="$E3DC_USER" \
  -e E3DC_PASSWORD="$E3DC_PASSWORD" \
  -e E3DC_AES_PASSWORD="$E3DC_AES_PASSWORD" \
  e3dcset -r EMS_BAT_SOC
```

---

**Kubernetes:**
```yaml
apiVersion: v1
kind: Secret
metadata:
  name: e3dc-credentials
type: Opaque
stringData:
  user: "user@beispiel.de"
  password: "portal_password"
  aes_password: "rscp_password"

---

apiVersion: v1
kind: Pod
metadata:
  name: e3dcset
spec:
  containers:
  - name: e3dcset
    image: e3dcset:latest
    env:
    - name: E3DC_USER
      valueFrom:
        secretKeyRef:
          name: e3dc-credentials
          key: user
    - name: E3DC_PASSWORD
      valueFrom:
        secretKeyRef:
          name: e3dc-credentials
          key: password
    - name: E3DC_AES_PASSWORD
      valueFrom:
        secretKeyRef:
          name: e3dc-credentials
          key: aes_password
```

---

### Priorität

**Reihenfolge (höchste zuerst):**
1. **Umgebungsvariablen** (überschreiben alles)
2. **Custom Config** (`-p /pfad/zu/config`)
3. **Standard Config** (`e3dcset.config`)

**Beispiel:**
```bash
# Config-Datei hat: e3dc_user=old@example.com
# Environment hat: E3DC_USER=new@example.com

./e3dcset -r EMS_BAT_SOC
# → Nutzt: new@example.com (Environment gewinnt!)
```

---

## Tags-Datei Format

### Übersicht

Die Tags-Datei definiert benutzerdefinierte RSCP-Tag-Namen und Interpretationen.

**Standard-Pfad:** `e3dcset.tags`

**Custom-Pfad:**
```bash
./e3dcset -t /pfad/zu/custom.tags -r MY_TAG
```

---

### Struktur

```ini
# Kommentare beginnen mit #

[KATEGORIE]
TAG_NAME = 0xHEXVALUE # Beschreibung (optional)

[INTERPRETATIONS]
0xHEXVALUE:WERT = Interpretationstext
```

---

### Kategorien

Mögliche Kategorien (Sections):
- `[EMS]` – Energiemanagementsystem
- `[BAT]` – Batterie
- `[PVI]` – PV-Wechselrichter
- `[PM]` – Leistungsmesser
- `[WB]` – Wallbox
- `[DCDC]` – DC/DC-Wandler
- `[INFO]` – Systeminformationen
- `[DB]` – Datenbank / Historie

---

### Tag-Definitionen

**Syntax:**
```ini
[KATEGORIE]
TAG_NAME = 0xHEXVALUE # Beschreibung
```

**Beispiel:**
```ini
[EMS]
EMS_POWER_PV = 0x01000001 # PV-Leistung in Watt
EMS_BAT_SOC = 0x01000008 # Batterie-Ladezustand in Prozent
EMS_MODE = 0x01000011 # EMS-Betriebsmodus

[BAT]
BAT_REQ_RSOC = 0x02040001 # Relativer SOC (Portal-Anzeige)
BAT_REQ_ASOC = 0x02040002 # Absoluter SOC / State of Health
```

---

### Interpretationen

**Syntax:**
```ini
[INTERPRETATIONS]
0xHEXVALUE:WERT = Beschreibungstext
```

**Beispiel:**
```ini
[INTERPRETATIONS]
# EMS-Modus
0x01000011:0 = Normal/Automatik
0x01000011:1 = Leerlauf
0x01000011:2 = Entladung
0x01000011:3 = Ladung
0x01000011:4 = Netzladung

# PVI Status
0x03040001:0 = Aus
0x03040001:1 = Initialisierung
0x03040001:2 = Online
0x03040001:3 = Fehler
```

---

### Vollständiges Beispiel

**Datei:** `my-custom.tags`
```ini
# Meine Custom E3DC Tags

[EMS]
EMS_POWER_PV = 0x01000001 # PV-Produktion
EMS_POWER_BAT = 0x01000002 # Batterie-Leistung (+laden/-entladen)
EMS_POWER_HOME = 0x0100000C # Hausverbrauch
EMS_POWER_GRID = 0x0100000D # Netz (+Bezug/-Einspeisung)
EMS_BAT_SOC = 0x01000008 # Batterie SOC
EMS_MODE = 0x01000011 # Betriebsmodus

[BAT]
BAT_REQ_RSOC = 0x02040001 # Relativer SOC
BAT_REQ_ASOC = 0x02040002 # State of Health
BAT_REQ_CHARGE_CYCLES = 0x02040003 # Ladezyklen
BAT_REQ_CURRENT = 0x02040004 # Strom in Ampere

[INTERPRETATIONS]
# EMS-Modi
0x01000011:0 = Automatik
0x01000011:1 = Standby
0x01000011:2 = Batterie-Entladung
0x01000011:3 = Batterie-Ladung
0x01000011:4 = Netzladung aktiv

# Boolean-Werte
0x03040001:0 = Nein
0x03040001:1 = Ja
```

**Verwendung:**
```bash
./e3dcset -t my-custom.tags -r EMS_MODE
# Output: Tag 0x01000011: 0 (Automatik)

./e3dcset -t my-custom.tags -l 1
# Listet EMS-Tags aus custom.tags
```

---

## Sicherheit

### Config-Datei Berechtigungen

**⚠️ KRITISCH:** Config-Dateien enthalten Passwörter im Klartext!

**Empfohlene Berechtigungen:**
```bash
# Nur Owner kann lesen/schreiben
chmod 600 e3dcset.config

# Besitzer prüfen
ls -l e3dcset.config
# -rw------- 1 user user 234 Feb 10 12:00 e3dcset.config
```

**Besitzer ändern:**
```bash
chown user:user e3dcset.config
```

---

### Passwörter mit Sonderzeichen

Bei Passwörtern mit Sonderzeichen (`!`, `$`, `#`, `"`, `'`, etc.) in Quotes setzen:

**Nicht quotiert (kann fehlschlagen):**
```ini
e3dc_password=Pass!wort$123
```

**Quotiert (empfohlen):**
```ini
e3dc_password="Pass!wort$123"
```

**Oder:** Sonderzeichen escapen:
```ini
e3dc_password=Pass\!wort\$123
```

---

### Environment vs. Config-Datei

**Environment-Variablen sind sicherer für:**
- Container (keine Datei-Berechtigungen nötig)
- Shared-Hosting (keine weltlesbaren Dateien)
- CI/CD (Secrets aus Secret-Manager)

**Config-Datei ist praktischer für:**
- Lokale Installation
- Mehrere verschiedene Configs (Dev/Prod)
- Einfaches Backup

**Best Practice:**
- **Lokal:** Config-Datei mit `chmod 600`
- **Docker/K8s:** Environment-Variablen aus Secrets
- **CI/CD:** GitHub Secrets → Environment-Variablen

---

## Erweiterte Einstellungen

### Mehrere Configs

**Szenario:** Mehrere E3DC-Systeme oder Test/Prod-Umgebungen

```bash
# Production
./e3dcset -p ~/.config/e3dc-prod.conf -r EMS_BAT_SOC

# Test-System
./e3dcset -p ~/.config/e3dc-test.conf -r EMS_BAT_SOC
```

**Wrapper-Script:**
```bash
#!/bin/bash
# e3dc-prod.sh
exec /usr/local/bin/e3dcset -p /etc/e3dc/prod.conf "$@"
```

---

### Config-Validierung

**Fehlende Pflichtfelder prüfen:**
```bash
# Config laden und debug-mode aktivieren
./e3dcset -p myconfig.conf -r EMS_BAT_SOC
```

**Fehlermeldungen:**
```
Fehler: server_ip nicht in Config-Datei gesetzt!
Hinweis: Nutze Environment-Variable E3DC_SERVER_IP oder setze in Config.
```

---

### Performance-Tuning

**Schnelle Abfragen:**
```ini
timeout_seconds=3
max_retries=1
```

**Langsame Netzwerke / Historie:**
```ini
timeout_seconds=20
max_retries=5
```

**Debug bei Problemen:**
```ini
debug=1
timeout_seconds=30
```

---

### Config-Template

**Erstelle Template:**
```bash
cat > e3dcset.config.template << 'EOF'
# E3DC e3dcset Configuration Template
# Copy to e3dcset.config and fill in your values

server_ip=YOUR_E3DC_IP
server_port=5033
e3dc_user=YOUR_EMAIL
e3dc_password=YOUR_PASSWORD
aes_password=YOUR_RSCP_PASSWORD

# Optional
timeout_seconds=10
max_retries=3
debug=0
EOF
```

**Nutzen:**
```bash
cp e3dcset.config.template e3dcset.config
nano e3dcset.config
chmod 600 e3dcset.config
```

---

**Zurück zu:** [README](../README.md) | **Siehe auch:** [USAGE.md](USAGE.md), [BUILDING.md](BUILDING.md)
