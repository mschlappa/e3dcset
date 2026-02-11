# ARCHITECTURE – Code-Struktur & Design

Dokumentation der Code-Architektur, Modul-Übersicht und Design-Entscheidungen.

---

## Inhaltsverzeichnis

- [Projekt-Übersicht](#projekt-übersicht)
- [Verzeichnisstruktur](#verzeichnisstruktur)
- [Module](#module)
- [RSCP-Protokoll](#rscp-protokoll)
- [Datenfluss](#datenfluss)
- [Design-Entscheidungen](#design-entscheidungen)
- [Erweiterungen](#erweiterungen)

---

## Projekt-Übersicht

`e3dcset` ist ein **modulares Kommandozeilen-Tool** zur Steuerung von E3DC S10 Hauskraftwerken über das RSCP-Protokoll.

**Kern-Technologien:**
- **Sprache:** C++ (C++11)
- **Protokoll:** RSCP (RES Charge Protocol)
- **Verschlüsselung:** AES-256 (eigene Implementierung)
- **Kommunikation:** TCP Sockets (Port 5033)

**Design-Prinzipien:**
- **Modularität** – Klare Trennung von Verantwortlichkeiten
- **Testbarkeit** – Unit-Tests für kritische Funktionen
- **Portabilität** – Linux & macOS Support
- **Sicherheit** – AES-verschlüsselte Kommunikation

---

## Verzeichnisstruktur

```
e3dcset/
├── src/                    # Source-Dateien (.cpp)
│   ├── e3dcset.cpp         # Main-Funktion, Argument-Parsing
│   ├── config.cpp          # Config-Parsing, Validierung
│   ├── rscp_handler.cpp    # RSCP Request/Response Handling
│   ├── output.cpp          # Output-Formatierung (Text, JSON)
│   ├── history.cpp         # History-Daten, Timestamp-Handling
│   ├── RscpProtocol.cpp    # RSCP-Protokoll-Implementierung
│   ├── SocketConnection.cpp # Netzwerk-Kommunikation
│   └── AES.cpp             # AES-256-Verschlüsselung
│
├── include/                # Header-Dateien (.h)
│   ├── config.h
│   ├── rscp_handler.h
│   ├── output.h
│   ├── history.h
│   ├── constants.h         # Globale Konstanten
│   ├── RscpProtocol.h
│   ├── RscpTags.h          # Protokoll Tag-Konstanten
│   ├── RscpTypes.h         # Protokoll Datentyp-Definitionen
│   ├── SocketConnection.h
│   └── AES.h
│
├── tests/                  # Unit-Tests
│   ├── test_framework.h    # Custom Test-Framework
│   ├── test_config.c       # Config-Tests
│   ├── test_safe_string.c  # String-Handling-Tests
│   └── test_validation.c   # Input-Validierungs-Tests
│
├── docs/                   # Dokumentation
│   ├── USAGE.md
│   ├── CONFIGURATION.md
│   ├── BUILDING.md
│   ├── ARCHITECTURE.md     # Diese Datei
│   └── CHANGELOG.md
│
├── Makefile                # Build-System
├── README.md               # Projekt-Übersicht
├── e3dcset.config.example  # Config-Beispiel
└── e3dcset.tags            # Tag-Definitionen
```

---

## Module

### 1. Main & Argument-Parsing (`e3dcset.cpp`)

**Verantwortung:**
- Kommandozeilen-Argumente parsen (`getopt_long`)
- Globalen Kontext (`g_ctx`) initialisieren
- Workflow orchestrieren (Config → Connect → Execute → Disconnect)

**Wichtige Funktionen:**
```cpp
int main(int argc, char *argv[])
void usage()                    // Hilfe-Text ausgeben
void watchSignalHandler()       // Ctrl+C Handler für Watch-Mode
```

**Argument-Parsing:**
```cpp
static struct option long_options[] = {
    {"watch", no_argument, 0, 'w'},
    {"interval", required_argument, 0, 1},
    {0, 0, 0, 0}
};

while ((opt = getopt_long(argc, argv, "c:d:e:E:ap:r:...")) != -1) {
    switch (opt) {
        case 'c': g_ctx.ladeLeistung = atoi(optarg); break;
        case 'r': g_ctx.werteAbfragen = true; break;
        ...
    }
}
```

**Globaler Kontext:**
```cpp
typedef struct {
    // Flags
    bool leistungAendern;
    bool werteAbfragen;
    bool historieAbfrage;
    
    // Parameter
    int ladeLeistung;
    int entladeLeistung;
    char* configPath;
    
    // Output
    bool quietMode;
    bool jsonOutput;
} Context;

extern Context g_ctx;
```

---

### 2. Config-Parsing (`config.cpp` / `config.h`)

**Verantwortung:**
- Config-Datei parsen (INI-Format)
- Environment-Variablen auslesen
- Credentials validieren

**Wichtige Funktionen:**
```cpp
void readConfig()                   // Config-Datei laden
void parseConfigLine(char* line)    // Einzelne Zeile parsen
int parseInt(const char* value)     // Integer parsen
char* parseString(const char* value) // String parsen (mit Quotes)
void validateConfig()               // Pflichtfelder prüfen
```

**Config-Felder:**
```cpp
typedef struct {
    char* server_ip;
    int server_port;
    char* e3dc_user;
    char* e3dc_password;
    char* aes_password;
    int timeout_seconds;
    int max_retries;
    int debug;
} Config;

extern Config g_config;
```

**Priorität:**
1. Environment-Variablen (`E3DC_USER`, `E3DC_PASSWORD`, ...)
2. Config-Datei (`e3dcset.config`)
3. Defaults

**Beispiel:**
```cpp
// Environment überschreibt Config
if (getenv("E3DC_USER")) {
    free(g_config.e3dc_user);
    g_config.e3dc_user = strdup(getenv("E3DC_USER"));
}
```

---

### 3. RSCP Handler (`rscp_handler.cpp` / `rscp_handler.h`)

**Verantwortung:**
- RSCP-Requests aufbauen
- RSCP-Responses parsen
- Tag-basierte Abfragen durchführen
- Batterie-Modul-Abfragen (`BAT_REQ_DATA` Container)

**Wichtige Funktionen:**
```cpp
void sendTagRequest(uint32_t tag)          // Einzelnen Tag abfragen
void sendPowerControl()                    // Leistung setzen
void sendHistoryRequest(char* type)        // Historie abfragen
void handleBatteryRequest(uint32_t tag, uint16_t index) // BAT_REQ_*
void parseResponse(RscpProtocol* frame)    // Response dekodieren
```

**Tag-Handling:**
```cpp
// Tag-Namen → Hex-Wert
uint32_t getTagByName(const char* name);

// Prüfen ob REQUEST-Tag (zweites Byte < 0x80)
bool isRequestTag(uint32_t tag) {
    return ((tag >> 16) & 0xFF) < 0x80;
}
```

**Container-Handling:**
```cpp
// BAT_REQ_* Tags benötigen Container
RscpProtocol* batReqData = new RscpProtocol();
batReqData->createContainerData(BAT_REQ_DATA);
batReqData->appendValue(BAT_INDEX, index);
batReqData->appendValue(BAT_REQ_RSOC, None);
```

---

### 4. Output-Formatierung (`output.cpp` / `output.h`)

**Verantwortung:**
- Normale Text-Ausgabe
- JSON-Output generieren
- Quiet-Mode (nur Wert)
- Fehlerausgaben

**Wichtige Funktionen:**
```cpp
void printTagValue(uint32_t tag, Value value)      // Normal
void printTagValueJSON(uint32_t tag, Value value)  // JSON
void printHistoryData(HistoryData* data)           // Historie
void printHistoryJSON(HistoryData* data)           // Historie JSON
```

**JSON-Format:**
```cpp
// Tag-Abfrage
{"tag":"EMS_BAT_SOC","hex":"0x01000008","value":85,"unit":"%"}

// Historie
{
  "period": {"start":"2026-02-10","end":"2026-02-10"},
  "data": {
    "pv_production": 15.76,
    "battery_charge": 8.24,
    ...
  },
  "unit": "kWh"
}
```

---

### 5. History-Handling (`history.cpp` / `history.h`)

**Verantwortung:**
- Historische Daten-Requests aufbauen
- Timestamps konvertieren (Unix → YYYY-MM-DD)
- Sampling-Intervalle bestimmen
- Energie-Summen berechnen

**Wichtige Funktionen:**
```cpp
void queryHistory(const char* type, const char* date)
uint64_t parseDate(const char* dateStr)        // YYYY-MM-DD → Unix
char* formatTimestamp(uint64_t ts)             // Unix → YYYY-MM-DD
int getSamplingInterval(const char* type)      // day=900s, week=3600s, ...
```

**Sampling-Intervalle:**
```cpp
day   → 15 Minuten (900 Sekunden)
week  → 1 Stunde (3600 Sekunden)
month → 1 Tag (86400 Sekunden)
year  → 1 Woche (604800 Sekunden)
```

**Timestamp-Validierung:**
```cpp
bool validateTimestamp(uint64_t ts) {
    return ts >= TIMESTAMP_MIN && ts <= TIMESTAMP_MAX;
    // 1970-01-01 bis 2100-01-01
}
```

---

### 6. RSCP-Protokoll (`RscpProtocol.cpp` / `RscpProtocol.h`)

**Verantwortung:**
- RSCP-Frame-Struktur implementieren
- Serialisierung / Deserialisierung
- Tag-Value-Pairs verwalten
- Container-Handling (verschachtelte Tags)

**Frame-Struktur:**
```
[MAGIC (2 Bytes)] [CTRL (2 Bytes)] [LENGTH (4 Bytes)] [DATA] [CRC (4 Bytes)]
```

**Klasse:**
```cpp
class RscpProtocol {
public:
    void createFrame(uint32_t tag, DataType type, void* data);
    void appendValue(uint32_t tag, void* value);
    void createContainerData(uint32_t tag);
    void serialize(uint8_t* buffer);
    void deserialize(uint8_t* buffer, size_t length);
    
private:
    uint32_t tag;
    DataType dataType;
    void* data;
    size_t length;
};
```

**Data-Types:**
```cpp
TYPE_NONE       = 0x00
TYPE_BOOL       = 0x01
TYPE_CHAR8      = 0x02
TYPE_UCHAR8     = 0x03
TYPE_INT16      = 0x04
TYPE_UINT16     = 0x05
TYPE_INT32      = 0x06
TYPE_UINT32     = 0x07
TYPE_FLOAT32    = 0x08
TYPE_CONTAINER  = 0x0E
```

---

### 7. Socket-Kommunikation (`SocketConnection.cpp` / `.h`)

**Verantwortung:**
- TCP-Verbindung aufbauen
- Daten senden / empfangen
- Timeouts implementieren
- Retry-Logik

**Wichtige Funktionen:**
```cpp
int SocketConnect(const char* ip, int port)
int SocketSend(int socket, uint8_t* data, size_t length)
int SocketReceive(int socket, uint8_t* buffer, size_t length)
void SocketClose(int socket)
void SocketSetTimeout(int socket, int seconds)
```

**Timeout-Handling:**
```cpp
struct timeval timeout;
timeout.tv_sec = g_config.timeout_seconds;  // Standard: 10s
timeout.tv_usec = 0;
setsockopt(socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
```

**Retry-Logik:**
```cpp
for (int retry = 0; retry < g_config.max_retries; retry++) {
    int result = SocketConnect(ip, port);
    if (result >= 0) break;
    
    if (retry < g_config.max_retries - 1) {
        sleep(1);  // 1s warten vor Retry
    }
}
```

---

### 8. AES-Verschlüsselung (`AES.cpp` / `AES.h`)

**Verantwortung:**
- AES-256-Verschlüsselung implementieren
- RSCP-Kommunikation verschlüsseln/entschlüsseln
- Key-Derivation

**Klasse:**
```cpp
class AES {
public:
    AES(const uint8_t* key, size_t keyLength);
    void encrypt(uint8_t* data, size_t length);
    void decrypt(uint8_t* data, size_t length);
    
private:
    uint8_t expandedKey[240];
    void keyExpansion(const uint8_t* key);
    void encryptBlock(uint8_t* block);
    void decryptBlock(uint8_t* block);
};
```

**Nutzung:**
```cpp
// Verschlüsseln
AES aes((uint8_t*)g_config.aes_password, strlen(g_config.aes_password));
aes.encrypt(frameData, frameLength);
SocketSend(socket, frameData, frameLength);

// Entschlüsseln
SocketReceive(socket, buffer, bufferSize);
aes.decrypt(buffer, receivedLength);
```

---

## RSCP-Protokoll

### Protokoll-Übersicht

**RSCP (RES Charge Protocol):**
- Proprietäres E3DC-Protokoll
- TCP-basiert (Port 5033)
- AES-256-verschlüsselt
- Request/Response-Architektur

---

### Frame-Struktur

```
+--------+--------+----------+---------+--------+
| MAGIC  | CTRL   | LENGTH   | DATA    | CRC32  |
| 2 Byte | 2 Byte | 4 Byte   | N Byte  | 4 Byte |
+--------+--------+----------+---------+--------+
```

**MAGIC:** `0xE3DC` (konstant)  
**CTRL:** Flags (verschlüsselt, etc.)  
**LENGTH:** Länge der DATA-Section  
**DATA:** Tag-Value-Pairs  
**CRC32:** Checksumme

---

### Tag-Value-Pairs

```
+--------+----------+----------+---------+
| TAG    | DATATYPE | LENGTH   | VALUE   |
| 4 Byte | 1 Byte   | 2 Byte   | N Byte  |
+--------+----------+----------+---------+
```

**TAG:** Eindeutige ID (z.B. `0x01000008` = EMS_BAT_SOC)  
**DATATYPE:** Typ (INT32, FLOAT, Container, etc.)  
**LENGTH:** Länge des VALUE-Feldes  
**VALUE:** Eigentlicher Wert

---

### Tag-Namenskonvention

```
0x AABBCCDD
   │ │ │ │
   │ │ │ └─ Eindeutige ID innerhalb der Kategorie
   │ │ └─── REQUEST (00-7F) / RESPONSE (80-FF)
   │ └───── Unterkategorie
   └─────── Hauptkategorie (1=EMS, 2=BAT, ...)
```

**Beispiele:**
- `0x01000008` – EMS, REQUEST, SOC
- `0x01800008` – EMS, RESPONSE, SOC
- `0x02040001` – BAT, REQ_DATA, RSOC

**Regel:** Nur REQUEST-Tags (zweites Byte < 0x80) können abgefragt werden!

---

### Container-Tags

Container = verschachtelte Tag-Value-Pairs

**Beispiel:** `BAT_REQ_DATA`
```
BAT_REQ_DATA (0x02040000) {
    BAT_INDEX (0x02040001): 0
    BAT_REQ_RSOC (0x02040002): None  // Request ohne Wert
}
```

**Code:**
```cpp
RscpProtocol* container = new RscpProtocol();
container->createContainerData(BAT_REQ_DATA);
container->appendValue(BAT_INDEX, index);
container->appendValue(BAT_REQ_RSOC, None);
```

---

## Datenfluss

### Normale Tag-Abfrage (`-r EMS_BAT_SOC`)

```
1. main() parst Argumente
   └─ g_ctx.werteAbfragen = true
   └─ g_ctx.leseTag = 0x01000008

2. readConfig() lädt Config
   └─ g_config.server_ip = "192.168.1.100"

3. connectToServer() verbindet
   └─ SocketConnect(ip, 5033)
   └─ AES-Verschlüsselung initialisieren

4. mainLoop() startet Kommunikation
   └─ sendTagRequest(0x01000008)
      └─ RscpProtocol.createFrame(0x01000008, TYPE_NONE, NULL)
      └─ aes.encrypt(frame)
      └─ SocketSend(socket, frame)

5. Warten auf Response
   └─ SocketReceive(socket, buffer)
   └─ aes.decrypt(buffer)
   └─ RscpProtocol.deserialize(buffer)

6. Response parsen
   └─ parseResponse(frame)
   └─ printTagValue(tag, value)  // "Tag 0x01000008: 85"

7. Verbindung trennen
   └─ SocketClose(socket)
```

---

### Historie-Abfrage (`-H day`)

```
1. main() → g_ctx.historieAbfrage = true, g_ctx.historieTyp = "day"

2. mainLoop() → queryHistory("day", "today")

3. queryHistory() berechnet Zeitraum
   └─ Start: heute 00:00:00
   └─ Ende: heute 23:59:59
   └─ Intervall: 900s (15 Minuten)

4. RSCP-Request aufbauen
   └─ DB_REQ_HISTORY_DATA_DAY Container:
      ├─ DB_REQ_HISTORY_TIME_START: <timestamp_start>
      ├─ DB_REQ_HISTORY_TIME_INTERVAL: 900
      └─ DB_REQ_HISTORY_TIME_SPAN: 86400

5. Response enthält Array von Datenpunkten
   └─ parseHistoryResponse()
   └─ Energiesummen berechnen
   └─ printHistoryData()
```

---

## Design-Entscheidungen

### 1. Modulare Architektur

**Warum?**
- Testbarkeit (einzelne Module isoliert testen)
- Wartbarkeit (klare Verantwortlichkeiten)
- Erweiterbarkeit (neue Features einfach hinzufügen)

**Beispiel:** `output.cpp` kann JSON-Format ändern, ohne `rscp_handler.cpp` zu berühren.

---

### 2. Eigene AES-Implementierung

**Warum nicht OpenSSL?**
- **Portabilität:** Keine externe Dependency
- **Einfachheit:** Nur AES-256-CBC benötigt
- **Kontrolle:** Volle Kontrolle über Implementierung

**Nachteil:** Keine Hardware-Beschleunigung (AES-NI)

---

### 3. Globaler Kontext (`g_ctx`)

**Warum global?**
- Einfacher Zugriff aus allen Modulen
- Keine Pointer-Weitergabe durch alle Funktionen
- C-Style, kein komplexes OOP

**Nachteil:** Nicht thread-safe (aber nicht benötigt)

---

### 4. String-Handling mit C-Strings

**Warum nicht `std::string`?**
- Legacy-Code-Kompatibilität
- Einfacher Memory-Management (malloc/free)
- Kompatibel mit C-Test-Framework

**Safety:** `safe_strdup()` mit NULL-Checks

---

### 5. Automatisches Dependency-Tracking (Makefile)

**Warum?**
- Entwickler-Produktivität (kein `make clean` nötig)
- Korrekte Builds (Header-Änderungen → Rebuild)
- Standard-Best-Practice

---

## Erweiterungen

### Neue Tag-Abfragen hinzufügen

**1. Tag in `e3dcset.tags` definieren:**
```ini
[EMS]
MY_NEW_TAG = 0x01000099 # Mein neues Tag
```

**2. Optional: Interpretation hinzufügen:**
```ini
[INTERPRETATIONS]
0x01000099:0 = Status A
0x01000099:1 = Status B
```

**3. Nutzen:**
```bash
./e3dcset -r MY_NEW_TAG
```

**Kein Code-Change nötig!**

---

### Neuen Output-Format hinzufügen (z.B. XML)

**1. Funktion in `output.cpp` erstellen:**
```cpp
void printTagValueXML(uint32_t tag, Value value) {
    printf("<tag id=\"0x%08X\">\n", tag);
    printf("  <value>%d</value>\n", value);
    printf("</tag>\n");
}
```

**2. Flag in `e3dcset.cpp` hinzufügen:**
```cpp
case 'x':  // --xml
    g_ctx.xmlOutput = true;
    break;
```

**3. In `mainLoop()` aufrufen:**
```cpp
if (g_ctx.xmlOutput) {
    printTagValueXML(tag, value);
} else if (g_ctx.jsonOutput) {
    printTagValueJSON(tag, value);
} else {
    printTagValue(tag, value);
}
```

---

### Neue RSCP-Funktion implementieren

**Beispiel:** Wallbox-Steuerung

**1. Funktionen in `rscp_handler.cpp` erstellen:**
```cpp
void setWallboxCurrent(uint16_t ampere) {
    RscpProtocol* frame = new RscpProtocol();
    frame->createFrame(WB_REQ_SET_CURRENT, TYPE_UINT16, &ampere);
    sendFrame(frame);
}
```

**2. In `e3dcset.cpp` Flag hinzufügen:**
```cpp
case 'w':  // --wallbox-current
    g_ctx.wallboxCurrent = atoi(optarg);
    break;
```

**3. In `mainLoop()` aufrufen:**
```cpp
if (g_ctx.wallboxCurrent > 0) {
    setWallboxCurrent(g_ctx.wallboxCurrent);
}
```

---

## Code-Qualität

### Unit-Tests

**Abgedeckte Bereiche:**
- ✅ Config-Parsing (Integer, String, Validation)
- ✅ String-Handling (safe_strdup, Bounds-Checking)
- ✅ Input-Validation (Datum, Leistung, Energie)

**Nicht abgedeckt:**
- ❌ Socket-Kommunikation (erfordert Mock)
- ❌ RSCP-Protokoll (Integration-Tests)
- ❌ AES-Verschlüsselung (Vektor-Tests fehlen)

---

### Error-Handling

**Best Practices:**
- NULL-Pointer-Checks vor Zugriff
- Config-Validierung (Pflichtfelder)
- Socket-Timeouts (10s)
- Retry-Logik (max 3)
- DCB-Loop-Protection (max 3 Retries)

**Beispiel:**
```cpp
if (g_config.server_ip == NULL) {
    fprintf(stderr, "Fehler: server_ip nicht gesetzt!\n");
    fprintf(stderr, "Nutze Environment-Variable E3DC_SERVER_IP.\n");
    exit(EXIT_FAILURE);
}
```

---

### Memory-Management

**Regeln:**
- Jedes `malloc()` hat ein entsprechendes `free()`
- `safe_strdup()` für String-Allocation mit NULL-Check
- Cleanup bei Fehler-Exit

**Leak-Check:**
```bash
valgrind --leak-check=full ./e3dcset -r EMS_BAT_SOC
```

---

## Weitere Ressourcen

- **[USAGE.md](USAGE.md)** – CLI-Nutzung
- **[CONFIGURATION.md](CONFIGURATION.md)** – Config-Format
- **[BUILDING.md](BUILDING.md)** – Build-System

---

**Zurück zu:** [README](../README.md)
