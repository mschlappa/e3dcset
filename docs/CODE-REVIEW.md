# 🔍 e3dcset - Multi-Rollen Code Review

**Projekt:** e3dcset - E3DC S10 RSCP CLI Tool  
**Repository:** [github.com/mschlappa/e3dcset](https://github.com/mschlappa/e3dcset)  
**Review-Datum:** 10. Februar 2026  
**Reviewer:** JARVIS (Claude Sonnet 4.5)  
**Analysierte Dateien:** e3dcset.cpp (2373 Zeilen), RscpProtocol.cpp/h, SocketConnection.cpp/h, AES.cpp/h, Makefile

---

## Executive Summary

**Projekt-Bewertung:** Solider Proof-of-Concept mit deutlichem Verbesserungspotential

### Hauptstärken

- ✅ Funktionierendes RSCP-Protokoll-Implementierung
- ✅ Gute macOS-Portabilität (malloc.h → stdlib.h Fixes angewendet)
- ✅ Externes Tag-Management (e3dcset.tags) - flexibel und wartbar
- ✅ Multi-DCB-Unterstützung (neu implementiert, funktional)
- ✅ Umfangreiche Feature-Set (Historie, Batterie-Management, Realtime-Daten)

### Kritische Schwachstellen

- 🔴 **KRITISCH:** Passwörter in Klartext-Config (chmod 600 nicht ausreichend)
- 🟠 **HIGH:** Keine Eingabe-Validierung bei Config-Werten → Buffer Overflows möglich
- 🟠 **HIGH:** Memory Leaks in mehreren Code-Pfaden
- 🟡 **MEDIUM:** Fehlende Tests, manuelle Verifizierung erforderlich
- 🟡 **MEDIUM:** Fehlerbehandlung inkonsistent (manche Errors werden ignoriert)

**Gesamtbewertung:** **6.5/10** - Funktional, aber Sicherheits- und Robustheit-Verbesserungen dringend empfohlen

---

## 🔍 Rolle 1: Fachexperte (E3DC/RSCP/Energiespeicher)

### Positive Findings

#### ✅ RSCP-Protokoll korrekt implementiert

Die Basis-Implementierung des RSCP-Protokolls ist korrekt:

- Magic Header (0xDCE3) korrekt gesetzt
- AES-256-CBC Verschlüsselung funktional
- Frame-Parsing mit CRC-Validierung
- Container-Struktur korrekt verschachtelt
- Timestamp-Handling für Linux/macOS korrekt

#### ✅ Multi-DCB-Unterstützung (neu implementiert)

Die Implementierung der Multi-DCB-Abfrage folgt der korrekten RSCP-Syntax:

- 1. Request: `BAT_REQ_DCB_COUNT` holt Anzahl DCBs
- 2+ Requests: `BAT_REQ_DCB_INFO` mit Index als Wert (korrekte Syntax!)
- Multi-Request-Loop mit State-Machine (needMoreDCBRequests)
- Korrekte Terminierung nach letztem DCB

```cpp
// Korrekte RSCP-Syntax (NACH Fix):
protocol.appendValue(&container, TAG_BAT_REQ_DCB_INFO, (uint8_t)dcbIndex);
```

### Findings & Verbesserungsvorschläge

#### 🟠 HIGH: Fehlende RSCP Error-Codes Behandlung

**Problem:** Das Tool behandelt nur `RSCP::eTypeError`, aber nicht spezifische E3DC-Fehlercodes:

- `RSCP_ERR_ACCESS_DENIED` (0x02) - Zugriff verweigert
- `RSCP_ERR_NOT_AVAILABLE` (0x06) - Tag nicht verfügbar auf diesem System
- `RSCP_ERR_UNKNOWN_TAG` (0x07) - Unbekannter Tag

**Auswirkung:** User bekommt nur "Error code 6" statt "TAG_EMS_POWER_ADD nicht verfügbar (kein zusätzlicher PM installiert)"

**Empfehlung:**

```cpp
const char* getRscpErrorDescription(uint32_t errorCode) {
    switch(errorCode) {
        case RSCP_ERR_NOT_HANDLED: return "Nicht behandelt";
        case RSCP_ERR_ACCESS_DENIED: return "Zugriff verweigert";
        case RSCP_ERR_NOT_AVAILABLE: return "Nicht verfügbar auf diesem System";
        case RSCP_ERR_UNKNOWN_TAG: return "Unbekannter Tag";
        default: return "Unbekannter Fehler";
    }
}
```

#### 🟡 MEDIUM: Unvollständige Tag-Coverage

**Problem:** Wichtige E3DC-Features fehlen:

- **Wallbox-Steuerung:** `TAG_WB_REQ_SET_MODE`, `TAG_WB_REQ_SET_EXTERN` (für Solar-Laden)
- **Emergency Power:** `TAG_EP_REQ_IS_READY_FOR_SWITCH`, `TAG_EP_REQ_IS_ISLAND_GRID`
- **PVI (PV-Wechselrichter):** `TAG_PVI_REQ_COS_PHI`, `TAG_PVI_REQ_VOLTAGE_MONITORING`
- **System-Info:** `TAG_INFO_REQ_SW_RELEASE`, `TAG_INFO_REQ_SERIAL_NUMBER`

**Empfehlung:** Erweitere `e3dcset.tags` mit diesen Tags. Sie sind in `RscpTags.h` bereits definiert!

#### 🟡 MEDIUM: History-Daten: Fehlende Zeitstempel-Validierung

**Problem:** Bei History-Abfragen werden Zeitstempel nicht validiert:

```cpp
// Aktueller Code (Zeile 1688):
time_t valTime = g_ctx.historieStartTime + (rawCounter * g_ctx.historieInterval);
// Keine Prüfung ob valTime > endTime!
```

**Auswirkung:** Bei ungültigen Responses könnte der Code über das Datenende hinaus iterieren.

**Empfehlung:**

```cpp
time_t valTime = g_ctx.historieStartTime + (rawCounter * g_ctx.historieInterval);
time_t endTime = g_ctx.historieStartTime + g_ctx.historieSpan;
if (valTime > endTime) {
    DEBUG("Zeitstempel außerhalb des Abfrage-Bereichs: %ld > %ld\n", valTime, endTime);
    break;  // Stop processing
}
```

#### 🔵 LOW: Manuelle Ladung: Inkonsistente Terminologie

**Problem:** Der Parameter `-e` heißt "Ladungsmenge" (klingt wie Kapazität), meint aber "Energie in Wh zum Laden".

**Verwechslungsgefahr:**

- User erwartet: "Lade 5000 Wh" = "Lade bis SOC erreicht ist"
- Tatsächlich: "Lade MIT 5000 Wh aus Netz" (falls genug Zeit/Leistung)

**Empfehlung:** Rename zu `--charge-energy` oder `--grid-charge` + Doku verbessern.

#### ℹ️ INFO: Wrapper-Script fehlt (e3dcset-query.sh nicht lesbar)

**Beobachtung:** Das Wrapper-Script ist root-owned (chmod 600), konnte nicht gelesen werden.

**Annahme:** Es filtert vermutlich Write-Befehle (-c, -d, -e) für Read-Only-Nutzung.

**Empfehlung:** Script sollte für Analyse verfügbar sein (chmod 644) oder dokumentiert werden.

### Domänen-Expertise: Was fehlt?

| Feature | Status | Kommentar |
|---------|--------|-----------|
| Batterie-Kalibrierung | ❌ Fehlt | `TAG_EMS_REQ_START_ADJUST_BATTERY_VOLTAGE` vorhanden, aber nicht implementiert |
| Emergency Power Test | ❌ Fehlt | `TAG_EMS_REQ_START_EMERGENCYPOWER_TEST` - wichtig für Notstrom-Checks |
| Idle Periods (Ruhezeiten) | ❌ Fehlt | `TAG_EMS_REQ_GET_IDLE_PERIODS` - nützlich für Lastmanagement |
| Generator-Steuerung | ❌ Fehlt | `TAG_EMS_REQ_GET_GENERATOR_STATE` - für S10 E PRO mit Generator |
| System Reboot | ❌ Fehlt | `TAG_SYS_REQ_SYSTEM_REBOOT` - gefährlich, aber manchmal nötig |

---

## 🏗️ Rolle 2: Architekt

### Code-Struktur & Modularität

#### 🟠 HIGH: Monolithische main-Datei (2373 Zeilen)

**Problem:** `e3dcset.cpp` enthält ALLES:

- CLI-Parsing
- Config-Handling
- RSCP Request-Building
- Response-Handling (700+ Zeilen in `handleResponseValue`)
- History-Formatierung
- Tag-Management

**Auswirkung:** Schwer wartbar, schwer testbar, hohe Kopplung

**Empfehlung:** Modularisierung in separate Dateien:

```
src/
  cli/
    ArgumentParser.cpp/h     # CLI-Parsing
    ConfigManager.cpp/h      # Config-Datei lesen
  rscp/
    RequestBuilder.cpp/h     # createRequestExample auslagern
    ResponseHandler.cpp/h    # handleResponseValue auslagern
  tags/
    TagManager.cpp/h         # loadTagsFile, getTagByName
  history/
    HistoryFormatter.cpp/h   # History-Output-Logik
  main.cpp                   # Nur Orchestrierung
```

#### 🟡 MEDIUM: Globale Variablen statt Objekt-orientiertes Design

**Problem:** Viele globale Variablen:

```cpp
static int iSocket = -1;
static int iAuthenticated = 0;
static AES aesEncrypter;
static AES aesDecrypter;
static uint8_t ucEncryptionIV[AES_BLOCK_SIZE];
static e3dc_config_t e3dc_config;
static CommandContext g_ctx;
std::map<int, std::vector<TagInfo>> loadedTags;
```

**Auswirkung:** Nicht thread-safe, schwer testbar, versteckte Abhängigkeiten

**Empfehlung:** Objekt-orientierter Ansatz:

```cpp
class E3DCConnection {
private:
    int socket;
    bool authenticated;
    AES encrypter, decrypter;
    uint8_t encryptionIV[AES_BLOCK_SIZE];
    
public:
    E3DCConnection(const Config& cfg);
    bool connect();
    Response sendRequest(const Request& req);
    ~E3DCConnection();
};
```

#### ✅ Separation of Concerns: RSCP-Protokoll

Gute Trennung zwischen:

- `RscpProtocol.cpp/h` - Frame-Handling, Container-Parsing
- `SocketConnection.cpp/h` - Netzwerk-Layer
- `AES.cpp/h` - Verschlüsselung

Diese Module sind wiederverwendbar und gut testbar!

#### 🟡 MEDIUM: Fehlende Abstraktion für Tag-Handling

**Problem:** Jeder neue Tag erfordert Code-Änderungen in `handleResponseValue`:

```cpp
switch(response->tag){
    case TAG_EMS_POWER_PV: { ... }
    case TAG_EMS_POWER_BAT: { ... }
    case TAG_BAT_DATA: { ... }
    // 50+ weitere Cases...
}
```

**Empfehlung:** Tag-Handler-Registry mit Callbacks:

```cpp
class TagHandler {
public:
    virtual void handle(RscpProtocol* proto, SRscpValue* response) = 0;
};

class TagRegistry {
    std::map<uint32_t, std::unique_ptr<TagHandler>> handlers;
public:
    void registerHandler(uint32_t tag, TagHandler* handler);
    void dispatch(uint32_t tag, RscpProtocol* proto, SRscpValue* response);
};

// Verwendung:
registry.registerHandler(TAG_EMS_POWER_PV, new PowerPVHandler());
registry.dispatch(response->tag, &protocol, response);
```

#### 🔵 LOW: Build-System: Keine Dependency-Tracking

**Problem:** Makefile kompiliert immer alles neu (clean + compile):

```makefile
$(ROOT_VALUE): clean
    $(CXX) -O3 e3dcset.cpp RscpProtocol.cpp AES.cpp SocketConnection.cpp -o $@
```

**Auswirkung:** Langsame Builds bei großen Projekten, keine Incremental Compilation

**Empfehlung:** Proper Dependencies + Object-Files:

```makefile
OBJS = e3dcset.o RscpProtocol.o AES.o SocketConnection.o

$(ROOT_VALUE): $(OBJS)
    $(CXX) -O3 -o $@ $^

%.o: %.cpp %.h
    $(CXX) -O3 -c $< -o $@

clean:
    rm -f $(ROOT_VALUE) $(OBJS)
```

### Erweiterbarkeit

#### ✅ Externes Tag-Management (e3dcset.tags)

Hervorragende Design-Entscheidung:

- Neue Tags ohne Neukompilierung
- User-definierte Interpretationen
- Einfache Updates bei neuen E3DC-Firmware-Versionen

Beispiel aus `e3dcset.tags`:

```ini
[EMS]
EMS_POWER_PV = 0x01000001 # PV-Leistung in Watt

[INTERPRETATIONS]
0x01000009:0 = DC-gekoppelt
0x01000009:2 = AC-gekoppelt
```

#### ℹ️ INFO: Command-Context-Architektur

**Bewertung:** Die `CommandContext` Struktur ist ein guter Ansatz zur Zustandsverwaltung, aber könnte verbessert werden:

- ✅ Kapselt alle CLI-Optionen
- ✅ Default-Werte im Constructor
- ⚠️ Zu viele Flags (22 Booleans + diverse Daten)
- ⚠️ Mixing von "Was" (werteAbfragen) und "Wie" (quietMode)

**Empfehlung:** Auftrennen in `CommandOptions` (CLI) und `ExecutionContext` (Runtime-State)

---

## 👨‍💻 Rolle 3: Entwickler (Code-Qualität & Bugs)

### Critical Bugs

#### 🔴 CRITICAL: Buffer Overflow in Config-Parsing

**Location:** `readConfig()`, Zeile ~2110

**Problem:**

```cpp
char var[128], value[128], line[256];
if(sscanf(line, "%[^ \t=]%*[\t ]=%*[\t ]%[^\n]", var, value) == 2) {
    if(strcmp(var, "server_ip") == 0)
        strcpy(e3dc_config.server_ip, value);  // ⚠️ Kein Längencheck!
}
```

**Exploitation:** Eine Config-Zeile `server_ip=AAAA...AAAA` (>20 Zeichen) überschreibt Stack-Memory!

**Auswirkung:** Arbitrary Code Execution möglich

**Fix:**

```cpp
// Sicher:
if(strcmp(var, "server_ip") == 0) {
    size_t len = strlen(value);
    if (len >= sizeof(e3dc_config.server_ip)) {
        fprintf(stderr, "FEHLER: server_ip zu lang (max %zu Zeichen)\n", 
                sizeof(e3dc_config.server_ip) - 1);
        exit(EXIT_FAILURE);
    }
    strncpy(e3dc_config.server_ip, value, sizeof(e3dc_config.server_ip) - 1);
    e3dc_config.server_ip[sizeof(e3dc_config.server_ip) - 1] = '\0';
}

// Noch besser: strncpy überall ersetzen durch:
#define SAFE_STRCPY(dest, src) do { \
    strncpy(dest, src, sizeof(dest) - 1); \
    dest[sizeof(dest) - 1] = '\0'; \
} while(0)

SAFE_STRCPY(e3dc_config.server_ip, value);
```

#### 🟠 HIGH: Memory Leak: strdup() ohne free()

**Location:** Mehrere Stellen

**Problem:**

```cpp
// In main():
case 'p':
    g_ctx.configPath = strdup(optarg);  // ⚠️ Nie freigegeben!
    break;
case 't':
    g_ctx.tagfilePath = strdup(optarg); // ⚠️ Nie freigegeben!
    break;
case 'H':
    g_ctx.historieTyp = strdup(optarg); // ⚠️ Nie freigegeben!
    break;
```

**Auswirkung:** Bei häufiger Ausführung (z.B. in Cron-Job alle 5 Min) → Memory-Leak über Zeit

**Fix:** Vor `return 0` in `main()` aufräumen:

```cpp
// Am Ende von main():
if (g_ctx.configPath) free(g_ctx.configPath);
if (g_ctx.tagfilePath) free(g_ctx.tagfilePath);
if (g_ctx.historieTyp) free(g_ctx.historieTyp);
if (g_ctx.historieDatum) free(g_ctx.historieDatum);
if (g_ctx.tagName) free(g_ctx.tagName);
return 0;
```

#### 🟠 HIGH: Race Condition: localtime() überschreibt statischen Buffer

**Location:** History-Response-Handler, Zeile ~1565

**Problem:**

```cpp
// FALSCH (alter Code):
struct tm *pStartTm = localtime(&startTime);
struct tm *pEndTm = localtime(&endTime);  // ⚠️ Überschreibt pStartTm!

strftime(startStr, sizeof(startStr), "%d.%m.%Y", pStartTm);  // ❌ Kaputt!
```

**Bereits gefixt:**

```cpp
// GUT (aktueller Code):
struct tm startTm, endTm;
struct tm *pStartTm = localtime(&startTime);
if (pStartTm) {
    startTm = *pStartTm;  // SOFORT kopieren!
    strftime(startStr, sizeof(startStr), "%d.%m.%Y", &startTm);
}
struct tm *pEndTm = localtime(&endTime);
if (pEndTm) {
    endTm = *pEndTm;  // SOFORT kopieren!
    strftime(endStr, sizeof(endStr), "%d.%m.%Y", &endTm);
}
```

**Status:** ✅ Bereits behoben! Aber zeigt gute C-Awareness.

### Code-Qualität

#### ✅ macOS-Kompatibilität

Korrekte Behandlung von Platform-Unterschieden:

```cpp
// RscpProtocol.cpp, Zeile 23:
#ifdef __APPLE__
#include <stdlib.h>
#else
#include <malloc.h>
#endif

// Timestamp-Handling (Zeile 30):
#if defined(__linux__) || defined(__APPLE__)
    struct timeval timeVal;
    gettimeofday(&timeVal, NULL);
    frame->header.timestamp.seconds = timeVal.tv_sec;
    frame->header.timestamp.nanoseconds = timeVal.tv_usec * 1000;
#elif defined(WINNT)
    // Windows-Implementierung
#endif
```

#### 🟡 MEDIUM: Inkonsistente Fehlerbehandlung

**Problem:** Manche Fehler werden behandelt, andere ignoriert:

```cpp
// GUT (History-Handler, Zeile 1540):
if(historyData[i].dataType == RSCP::eTypeError) {
    uint32_t uiErrorCode = protocol->getValueAsUInt32(&historyData[i]);
    printf("Fehler: Tag 0x%08X, Code %u\n", historyData[i].tag, uiErrorCode);
    continue;  // ✅ Fehler behandelt
}

// SCHLECHT (Generic Handler, Default-Case):
default:
    printf("Unknown tag %08X\n", response->tag);  // ⚠️ Kein Exit-Code!
    break;
```

**Empfehlung:** Konsistente Error-Strategie:

- Kritische Fehler (Auth, Connection) → `exit(EXIT_FAILURE)`
- Erwartbare Fehler (Tag nicht verfügbar) → stderr + `return -1`
- Unbekannte Tags → stderr + weiter (Best-Effort)

#### 🟡 MEDIUM: Fehlende Null-Pointer-Checks

**Problem:** Mehrere Dereferenzierungen ohne Null-Check:

```cpp
// Zeile 958:
const char* getTagDescription(uint32_t tag) {
    for (auto& categoryPair : loadedTags) {
        for (auto& tagInfo : categoryPair.second) {
            if (tagInfo.hex == requestTag) {
                return tagInfo.description.c_str();  // ⚠️ Was wenn leer?
            }
        }
    }
    return NULL;  // ✅ Gut
}

// Verwendung (Zeile 1200):
const char* label = getTagDescription(tagValuePair.first);
if (label) {  // ✅ Gut - Check vorhanden!
    printf("%s\n", label);
}
```

**Status:** Meistens korrekt, aber nicht überall. Review empfohlen!

#### 🔵 LOW: Magic Numbers statt Constants

**Problem:** Viele Magic Numbers im Code:

```cpp
// Zeile 1141:
if ((tag & 0xFFF00000) == 0x03800000) {  // ⚠️ Was bedeutet das?

// Besser:
#define RSCP_TAG_NAMESPACE_MASK  0xFF000000
#define RSCP_TAG_BATTERY         0x03000000
#define RSCP_TAG_RESPONSE_BIT    0x00800000

if ((tag & RSCP_TAG_NAMESPACE_MASK) == RSCP_TAG_BATTERY &&
    (tag & RSCP_TAG_RESPONSE_BIT) != 0) {
```

### Best Practices

#### ✅ Debug-Makro mit Conditional Compilation

```cpp
#define DEBUG(...)if(debug) {printf(__VA_ARGS__);}
```

Ermöglicht Debug-Output ohne Performance-Impact im Release-Build!

#### ℹ️ INFO: Compiler Warnings

Test mit `-Wall -Wextra -Wpedantic`:

```bash
g++ -Wall -Wextra -Wpedantic -O3 e3dcset.cpp RscpProtocol.cpp AES.cpp SocketConnection.cpp -o e3dcset

# Vermutliche Warnings:
# - Unused variables (counter in mainLoop)
# - Signed/unsigned comparison in loops
# - Potential null pointer dereference
```

**Empfehlung:** Makefile mit Warnings erweitern!

---

## 🧪 Rolle 4: Tester

### Test-Status

#### 🟠 HIGH: KEINE Tests vorhanden!

**Beobachtung:** Keine Test-Dateien, kein Test-Framework, keine CI/CD

**Auswirkung:** Jede Änderung erfordert manuelle Verifikation mit echtem E3DC-System!

**Empfehlung:** Minimale Test-Abdeckung implementieren:

```
tests/
  unit/
    test_config_parser.cpp      # Config-Parsing ohne E3DC
    test_tag_manager.cpp         # Tag-Loading und Lookup
    test_response_parser.cpp     # Mock RSCP-Responses
  integration/
    test_e3dc_connection.cpp     # Echter E3DC-Zugriff (optional)
  mocks/
    mock_rscp_responses.bin      # Aufgezeichnete echte Responses
```

### Edge Cases & Robustheit

#### 🟠 HIGH: Netzwerk-Fehlerbehandlung: Connection Loss nicht getestet

**Szenario:** E3DC während laufender Abfrage offline geht

**Aktuelles Verhalten:**

```cpp
// receiveLoop(), Zeile 1831:
if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
    printf("Response receive timeout (retry)\n");
    break;  // ⚠️ Geht zurück zum mainLoop - was passiert dann?
}
else if(iResult == 0) {
    printf("Connection closed by peer\n");
    bStopExecution = true;  // ✅ Gut
    break;
}
```

**Problem:** Bei Timeout wird nicht klar, ob noch Daten kommen könnten → Potentiell hängender Prozess!

**Test-Fälle:**

- ✅ Connection refused (beim Start) - wird behandelt
- ⚠️ Connection timeout (nach 3 Sekunden) - unklar
- ❌ Connection drop während Receive - nicht getestet
- ❌ Partielle Frames (Netzwerk-Paket-Loss) - nicht getestet

#### 🟡 MEDIUM: DCB-Multi-Request: Loop-Terminierung bei Fehlern

**Problem:** Was passiert, wenn DCB #2 von 5 einen Error zurückgibt?

```cpp
// handleResponseValue(), Zeile 1092:
if(batteryData[i].dataType == RSCP::eTypeError) {
    uint32_t uiErrorCode = protocol->getValueAsUInt32(&batteryData[i]);
    fprintf(stderr, "Fehler: Tag 0x%08X, Code %u\n", batteryData[i].tag, uiErrorCode);
    // ...
    g_ctx.batContainerQuery = false;
    return -1;  // ⚠️ Loop stoppt - DCB 3-5 werden NICHT abgefragt!
}
```

**Bessere Strategie:**

```cpp
if(batteryData[i].dataType == RSCP::eTypeError) {
    fprintf(stderr, "Warnung: DCB #%u nicht verfügbar (Error %u)\n", 
            g_ctx.currentDCBIndex, uiErrorCode);
    // Markiere als "skip" aber continue mit nächstem DCB
    g_ctx.currentDCBIndex++;
    if (g_ctx.currentDCBIndex >= g_ctx.totalDCBs) {
        g_ctx.needMoreDCBRequests = false;
    }
    return 0;  // Weiter mit Loop
}
```

#### 🟡 MEDIUM: Config-File: Fehlende Validierung

**Test-Fall:** Was passiert bei ungültigen Config-Werten?

| Eingabe | Erwartetes Verhalten | Tatsächliches Verhalten |
|---------|---------------------|-------------------------|
| `server_port=999999` | Error: Port ungültig | ⚠️ atoi() → undefiniert (wraps to 16959?) |
| `server_ip=not.an.ip` | Error: Ungültige IP | ⚠️ Erst bei connect() → "Connection failed" |
| `debug=yes` | atoi("yes") = 0 → Debug OFF | ✅ Funktioniert (aber irreführend) |
| `MIN_LEISTUNG=-500` | Error: Negativ | ⚠️ Wird zu 4294966796 (uint wrap) |

**Empfehlung:** Config-Validierung nach dem Parsen!

### Test-Szenarien (empfohlen)

| Kategorie | Test-Fall | Priority |
|-----------|-----------|----------|
| **Netzwerk** | E3DC nicht erreichbar (offline) | 🔴 P1 |
| | Falsche IP-Adresse | 🔴 P1 |
| | Connection Timeout (3s) | 🟠 P2 |
| | Connection Drop während Receive | 🟠 P2 |
| **Authentifizierung** | Falsches Passwort | 🔴 P1 |
| | Falscher AES-Key | 🔴 P1 |
| | User ohne Zugriff | 🟠 P2 |
| **RSCP-Protokoll** | Tag nicht verfügbar (Error 0x06) | 🔴 P1 |
| | Access Denied (Error 0x02) | 🟠 P2 |
| | Unknown Tag (Error 0x07) | 🟠 P2 |
| | Korrupte Frame (CRC Error) | 🟠 P2 |
| | Partielle Response (unvollständige Daten) | 🟡 P3 |
| **Multi-DCB** | System mit 0 DCBs | 🟠 P2 |
| | System mit 1 DCB (kein Loop nötig) | 🔴 P1 |
| | System mit 5+ DCBs | 🟠 P2 |
| **History** | Datum in Zukunft | 🟠 P2 |
| | Datum vor Installation | 🟠 P2 |
| | Ungültiges Datum (z.B. 2024-13-45) | 🔴 P1 |

---

## 🔒 Rolle 5: Security

### Critical Security Issues

#### 🔴 CRITICAL: Credentials in Plaintext Config-File

**Location:** `e3dcset.config`

**Problem:** Passwörter in Klartext gespeichert:

```ini
server_ip=192.168.1.100
server_port=5033
e3dc_user=marcus@example.com
e3dc_password=SuperSecret123    # ⚠️ PLAINTEXT!
aes_password=AnotherSecret456   # ⚠️ PLAINTEXT!
```

**Bedrohungen:**

- Jeder Prozess mit User-Rechten kann lesen (chmod 600 schützt nur vor anderen Usern)
- Backup-Tools kopieren Passwörter
- Git/Dropbox könnte Config syncen
- Forensics nach System-Compromise

**Empfehlungen:**

1. **Kurzfristig:** Nutze macOS Keychain:

```bash
# Passwort speichern:
security add-generic-password -a "e3dcset" -s "e3dc_password" -w "SuperSecret123"

# Im Code lesen:
char password[256];
FILE *pipe = popen("security find-generic-password -a 'e3dcset' -s 'e3dc_password' -w", "r");
if (pipe && fgets(password, sizeof(password), pipe)) {
    password[strcspn(password, "\n")] = 0;  // Remove newline
    strcpy(e3dc_config.e3dc_password, password);
}
pclose(pipe);
```

2. **Mittelfristig:** Umgebungsvariablen:

```bash
export E3DC_PASSWORD="SuperSecret123"
export E3DC_AES_KEY="AnotherSecret456"

// Im Code:
const char* pw = getenv("E3DC_PASSWORD");
if (pw) strcpy(e3dc_config.e3dc_password, pw);
```

3. **Langfristig:** OAuth2 oder Token-basierter Auth (wenn E3DC es unterstützt)

#### 🟠 HIGH: Debug-Output leakt Credentials

**Location:** `readConfig()`, Zeile 2125

**Problem:** Debug-Mode zeigt Passwörter (maskiert, aber trotzdem Leak):

```cpp
DEBUG("e3dc_user=%s\n", strlen(e3dc_config.e3dc_user) > 0 ? "***@***" : "");
DEBUG("e3dc_password=%s\n", strlen(e3dc_config.e3dc_password) > 0 ? "********" : "");
DEBUG("aes_password=%s\n", strlen(e3dc_config.aes_password) > 0 ? "********" : "");
```

**Besser, aber immer noch Leak:**

- Passwort-Länge ist sichtbar (8 vs 16 vs 32 Zeichen)
- Ob Passwort gesetzt ist (leer vs nicht-leer)

**Empfehlung:** Nie Credential-Info loggen:

```cpp
DEBUG("e3dc_user=[REDACTED]\n");
DEBUG("e3dc_password=[REDACTED]\n");
DEBUG("aes_password=[REDACTED]\n");
```

#### 🟠 HIGH: Input Validation: Command Injection möglich

**Location:** History-Datum-Parsing

**Problem:** Wenn User `-D` Parameter später in Shell-Command nutzt:

```bash
# User führt aus:
./e3dcset -H day -D "2024-01-01; rm -rf /"  # ⚠️ Shell Injection!

# Wenn Code später sowas macht (hypothetisch):
system("echo $datum > log.txt");  # ❌ GEFÄHRLICH!
```

**Aktueller Status:** Code nutzt `sscanf()` für Parsing - SICHER!

```cpp
if (sscanf(dateStr, "%d-%d-%d", &year, &month, &day) != 3) {
    fprintf(stderr, "Fehler: Ungültiges Datumsformat '%s'\n", dateStr);
    exit(EXIT_FAILURE);
}  // ✅ Keine Shell-Execution - GUT!
```

**Aber:** Validierung fehlt für Werte:

```bash
# Was passiert bei:
./e3dcset -H day -D "9999-99-99"  # ⚠️ Undefiniertes Verhalten!

# Fix:
if (year < 2000 || year > 2100 || month < 1 || month > 12 || day < 1 || day > 31) {
    fprintf(stderr, "Fehler: Datum außerhalb gültigem Bereich\n");
    exit(EXIT_FAILURE);
}
```

### Network Security

#### ✅ AES-256-CBC Verschlüsselung

Gute Krypto-Hygiene:

- AES-256 (nicht 128) - stark genug für Heimnetzwerk
- CBC-Mode mit IV (nicht ECB) - verhindert Pattern-Erkennung
- IV wird pro Block erneuert - verhindert Replay-Attacks (teilweise)

#### 🟡 MEDIUM: Keine TLS/SSL - Cleartext über LAN

**Problem:** AES-256 verschlüsselt nur die RSCP-Payload, NICHT den TCP-Transport!

**Bedrohung:**

- Man-in-the-Middle im gleichen LAN kann Traffic sehen
- Magic Header 0xDCE3 ist sichtbar (Erkennung von E3DC-Traffic)
- Metadata leaks (Timing, Größe, Frequenz der Requests)

**Akzeptabel weil:** E3DC sollte nur in vertrauenswürdigem Heimnetzwerk sein!

**Aber:** Wenn E3DC über Internet erreichbar (Port-Forwarding) → KRITISCH!

#### 🔵 LOW: Socket-Timeout: Denial-of-Service möglich

**Location:** `SocketConnection.cpp`, Zeile 37

**Problem:** Fixed 3-Sekunden-Timeout:

```cpp
struct timeval tv;
tv.tv_sec = 3;  // ⚠️ Hardcoded
tv.tv_usec = 0;
setsockopt(iSocket, SOL_SOCKET, SO_RCVTIMEO, ...);
```

**Szenario:** E3DC antwortet absichtlich langsam (>3s) → Tool hängt in Retry-Loop

**Empfehlung:** Konfigurierbarer Timeout + Max-Retries:

```cpp
// In Config:
timeout_seconds=5
max_retries=3

// Im Code:
int retry = 0;
while (retry < e3dc_config.max_retries) {
    if (recv_with_timeout(socket, buffer, size, e3dc_config.timeout_seconds) > 0) {
        break;  // Success
    }
    retry++;
    fprintf(stderr, "Retry %d/%d...\n", retry, e3dc_config.max_retries);
}
if (retry == e3dc_config.max_retries) {
    fprintf(stderr, "FEHLER: Maximale Retries erreicht\n");
    exit(EXIT_FAILURE);
}
```

### Wrapper-Script Security Review (ohne Zugriff)

#### ℹ️ INFO: e3dcset-query.sh - Annahmen basierend auf Datei-Permissions

**Beobachtung:** Script ist `root:wheel` owned, chmod 600

**Vermutete Funktionalität:** Wrapper für Read-Only-Abfragen

**Security-Empfehlungen (ohne Code gesehen zu haben):**

- ✅ `chmod 600` ist gut - nur Root kann lesen/ausführen
- ⚠️ Wenn Script sudo-Wrapper ist: Validiere ALLE Eingaben!
- ⚠️ Nutze `--` bei getopts um Option-Injection zu verhindern
- ⚠️ Quote ALLE Variablen: `"$var"` nicht `$var`
- ⚠️ Nutze `exec` statt `system()` wenn möglich

**Beispiel sicheres Wrapper-Pattern:**

```bash
#!/bin/bash
set -euo pipefail  # Fail on error, undefined vars, pipe failures

# Whitelist allowed operations
case "$1" in
    -r|-m|-H)
        # Read operations - allowed
        exec /usr/local/bin/e3dcset "$@"
        ;;
    -c|-d|-e|-E)
        echo "FEHLER: Schreib-Operationen nicht erlaubt" >&2
        exit 1
        ;;
    *)
        echo "FEHLER: Unbekannte Operation" >&2
        exit 1
        ;;
esac
```

---

## 📊 Priorisierte Verbesserungsvorschläge

### 🔴 KRITISCH (sofort beheben!)

1. **Config-File Security:** Passwörter aus Klartext-File entfernen → macOS Keychain oder Umgebungsvariablen
2. **Buffer Overflow Fix:** `strcpy()` durch `strncpy()` + Längen-Validierung ersetzen
3. **Input Validation:** Config-Werte validieren (IP-Format, Port-Range, etc.)

### 🟠 HIGH (kurzfristig)

1. **Memory Leaks:** `strdup()` Aufrufe aufräumen (free() in `main()`)
2. **Error Handling:** RSCP Error-Codes mit Beschreibungen ausgeben
3. **Netzwerk-Robustheit:** Connection-Loss während Receive behandeln
4. **Tests:** Minimale Unit-Tests für Config-Parsing und Tag-Lookup

### 🟡 MEDIUM (mittelfristig)

1. **Code-Refactoring:** `e3dcset.cpp` in Module aufteilen (CLI, RSCP, Handler)
2. **Fehlerbehandlung:** Konsistente Error-Strategie (Exit-Codes, stderr vs stdout)
3. **Build-System:** Makefile mit Dependency-Tracking und Compiler-Warnings
4. **DCB-Error-Handling:** Fehler in einzelnen DCBs nicht fatal machen
5. **Tag-Coverage:** Wallbox, Emergency Power, System-Info Tags hinzufügen

### 🔵 LOW (nice to have)

1. **Objekt-orientierter Ansatz:** Globale Variablen durch Klassen ersetzen
2. **Magic Numbers:** Durch Named Constants ersetzen
3. **Wrapper-Script:** Dokumentation + Code-Review
4. **Terminologie:** `-e Ladungsmenge` umbenennen zu `--charge-energy`

---

## ✨ Neue Features (Ideen)

| Feature | Priorität | Beschreibung |
|---------|-----------|--------------|
| Wallbox-Steuerung | 🔴 HIGH | Solar-Laden konfigurieren, Status abfragen |
| Emergency Power Test | 🟠 MEDIUM | Notstrom-Test manuell triggern |
| Idle Periods Management | 🟠 MEDIUM | Ruhezeiten setzen/abfragen (Lastmanagement) |
| JSON Output | 🟠 MEDIUM | `--json` Flag für maschinelles Parsing |
| Continuous Monitoring | 🔵 LOW | `--watch` Modus für Realtime-Überwachung |
| InfluxDB Export | 🔵 LOW | History-Daten direkt in Monitoring-System |

---

## 🎯 Gesamtbewertung

### Stärken

- ✅ Funktionierendes RSCP-Protokoll
- ✅ Gute macOS-Portabilität
- ✅ Flexibles Tag-Management
- ✅ Multi-DCB-Support (neu, funktional)
- ✅ Umfangreiche Features (History, Battery, Realtime)

### Schwächen

- 🔴 Security: Plaintext Credentials
- 🔴 Code-Qualität: Buffer Overflows möglich
- 🟠 Architektur: Monolithisch, schwer wartbar
- 🟠 Robustheit: Fehlende Error-Handling
- 🟡 Testing: Keine Tests

### Empfehlung

**Status Quo:** Funktional für persönlichen Gebrauch, NICHT für Production/Public Release

**Nächste Schritte:**

1. Security-Fixes (Credentials + Buffer Overflow) - **KRITISCH**
2. Memory-Leaks beheben - **HIGH**
3. Minimale Tests implementieren - **HIGH**
4. Code-Refactoring für Wartbarkeit - **MEDIUM**

**Fazit:** Solide Basis mit deutlichem Verbesserungspotential. Mit den empfohlenen Fixes wird das Tool production-ready! 🚀

---

**Review durchgeführt von:** JARVIS (Claude Sonnet 4.5)  
**Datum:** 10. Februar 2026  
**Methodik:** Multi-Rollen-Analyse (Fachexperte, Architekt, Entwickler, Tester, Security)  
**Analysierte LOC:** ~4000 Zeilen (e3dcset.cpp, RscpProtocol, SocketConnection, AES)
