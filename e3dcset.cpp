#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <strings.h>
#include <time.h>
#include <cmath>
#include <map>
#include <string>
#include <vector>
#include "RscpProtocol.h"
#include "RscpTags.h"
#include "SocketConnection.h"
#include "AES.h"

#define DEBUG(...)if(debug) {printf(__VA_ARGS__);}

#define AES_KEY_SIZE    32
#define AES_BLOCK_SIZE  32

// Default values for history queries (required by E3DC device)
// Intervals and spans are optimized per history type
#define HISTORY_INTERVAL_DAY      900     // 15 minutes
#define HISTORY_SPAN_DAY          86400   // 24 hours
#define HISTORY_INTERVAL_WEEK     3600    // 1 hour
#define HISTORY_SPAN_WEEK         604800  // 7 days
#define HISTORY_INTERVAL_MONTH    86400   // 1 day
#define HISTORY_SPAN_MONTH        2678400 // 31 days
#define HISTORY_INTERVAL_YEAR     604800  // 1 week
#define HISTORY_SPAN_YEAR         31536000// 365 days

typedef struct {

        uint32_t MIN_LEISTUNG;
        uint32_t MAX_LEISTUNG;
        uint32_t MIN_LADUNGSMENGE;
        uint32_t MAX_LADUNGSMENGE;
    char         server_ip[20];
    uint32_t server_port;
    char         e3dc_user[128];
    char         e3dc_password[128];
    char         aes_password[128];
    bool         debug;

} e3dc_config_t;

// Command Context - kapselt alle Kommandozeilen-bezogenen Zustände
struct CommandContext {
    // Control modes
    bool leistungAendern;
    bool automatischLeistungEinstellen;
    bool ladeLeistungGesetzt;
    bool entladeLeistungGesetzt;
    bool manuelleSpeicherladung;
    bool werteAbfragen;
    bool quietMode;
    bool listTags;
    int listCategory;
    bool historieAbfrage;
    bool batContainerQuery;  // True wenn BAT_REQ_* Tag abgefragt wird
    bool modulInfoDump;      // True wenn alle Modul-Werte abgefragt werden (-m)
    
    // Multi-DCB support
    bool needMoreDCBRequests;  // True wenn weitere DCB-Requests nötig sind
    uint8_t currentDCBIndex;   // Aktueller DCB-Index für Multi-Request
    uint8_t totalDCBs;         // Gesamtanzahl DCBs (aus DCB_COUNT)
    bool isFirstModuleDumpRequest;  // True für ersten Request (Battery-Level-Daten)
    
    // Power and energy settings
    uint32_t ladungsMenge;
    uint32_t ladeLeistung;
    uint32_t entladeLeistung;
    uint32_t leseTag;
    uint16_t batIndex;  // Batterie-Modul Index (0 = erstes Modul)
    
    // History query parameters
    char *historieDatum;        // Format: "YYYY-MM-DD" or "today"
    char *historieTyp;          // "day", "week", "month", "year"
    uint32_t historieInterval;  // Actual interval sent to device
    uint32_t historieSpan;      // Actual span sent to device
    time_t historieStartTime;   // Start timestamp for display
    
    // Configuration paths
    char *configPath;
    char *tagfilePath;
    char *tagName;  // Speichert Tag-Namen für spätere Konvertierung
    
    // Constructor with defaults
    CommandContext() : 
        leistungAendern(false),
        automatischLeistungEinstellen(false),
        ladeLeistungGesetzt(false),
        entladeLeistungGesetzt(false),
        manuelleSpeicherladung(false),
        werteAbfragen(false),
        quietMode(false),
        listTags(false),
        listCategory(0),
        historieAbfrage(false),
        batContainerQuery(false),
        modulInfoDump(false),
        needMoreDCBRequests(false),
        currentDCBIndex(0),
        totalDCBs(0),
        isFirstModuleDumpRequest(true),
        ladungsMenge(0),
        ladeLeistung(0),
        entladeLeistung(0),
        leseTag(0),
        batIndex(0),
        historieInterval(HISTORY_INTERVAL_DAY),
        historieSpan(HISTORY_SPAN_DAY),
        historieStartTime(0),
        configPath(strdup("e3dcset.config")),
        tagfilePath(strdup("e3dcset.tags")),
        tagName(NULL),
        historieDatum(NULL),
        historieTyp(NULL)
    {}
};

static int iSocket = -1;
static int iAuthenticated = 0;

static AES aesEncrypter;
static AES aesDecrypter;

static uint8_t ucEncryptionIV[AES_BLOCK_SIZE];
static uint8_t ucDecryptionIV[AES_BLOCK_SIZE];

static e3dc_config_t e3dc_config;

static bool debug = false;

// Globale Command Context Instanz
static CommandContext g_ctx;

// Tag-Kategorien als Enum statt Magic Numbers
enum TagCategory {
    CATEGORY_OVERVIEW = 0,
    CATEGORY_EMS = 1,
    CATEGORY_BAT = 2,
    CATEGORY_PVI = 3,
    CATEGORY_PM = 4,
    CATEGORY_WB = 5,
    CATEGORY_DCDC = 6,
    CATEGORY_INFO = 7,
    CATEGORY_DB = 8,
    CATEGORY_SYS = 9,
    CATEGORY_MAX = 10  // Für Validierung
};

// Kategorie-Deskriptoren
struct CategoryDescriptor {
    int id;
    const char* shortName;
    const char* fullName;
};

static const CategoryDescriptor categoryDescriptors[] = {
    {CATEGORY_EMS, "EMS", "Energy Management"},
    {CATEGORY_BAT, "BAT", "Battery"},
    {CATEGORY_PVI, "PVI", "PV Inverter"},
    {CATEGORY_PM, "PM", "Power Meter"},
    {CATEGORY_WB, "WB", "Wallbox"},
    {CATEGORY_DCDC, "DCDC", "DC/DC Converter"},
    {CATEGORY_INFO, "INFO", "System Info"},
    {CATEGORY_DB, "DB", "Database"},
    {CATEGORY_SYS, "SYS", "System"}
};
static const int NUM_CATEGORIES = sizeof(categoryDescriptors) / sizeof(categoryDescriptors[0]);

// Datenstrukturen für geladene Tags
struct TagInfo {
    std::string name;
    uint32_t hex;
    std::string description;
};

std::map<int, std::vector<TagInfo>> loadedTags;  // category -> tags
std::map<std::string, std::string> loadedInterpretations;  // "hex:value" -> interpretation

// DCB-Daten-Struktur für Multi-Request-Sammlung
struct DCBData {
    uint8_t index;
    std::vector<std::pair<uint32_t, SRscpValue>> tags;  // tag -> value pairs
};

// Batterie-Modul-Daten-Struktur
struct BatteryModuleData {
    std::map<uint32_t, SRscpValue> batteryTags;  // Battery-level tags (SOC, SOH, etc.)
    std::vector<DCBData> dcbs;  // Per-DCB data
    uint8_t dcbCount;
};

static BatteryModuleData g_batteryData;  // Global accumulator for module dump

// Forward declarations for helper functions
int sendRequestAndReceive(RscpProtocol* protocol, SRscpValue& rootValue);
int buildDCBRequest(RscpProtocol* protocol, SRscpFrameBuffer* frameBuffer, uint16_t batIndex, uint8_t dcbIndex);

// Tag-Datei laden
void loadTagsFile(const char* filename) {
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        fprintf(stderr, "FEHLER: Tag-Datei '%s' nicht gefunden!\n", filename);
        fprintf(stderr, "Das Tool benötigt die Tag-Definitions-Datei zum Betrieb.\n");
        fprintf(stderr, "Bitte stellen Sie sicher, dass 'e3dcset.tags' im aktuellen Verzeichnis vorhanden ist,\n");
        fprintf(stderr, "oder geben Sie den Pfad mit -t an.\n\n");
        exit(EXIT_FAILURE);
    }
    
    char line[512];
    int currentCategory = 0;
    
    while (fgets(line, sizeof(line), fp)) {
        // Kommentare und leere Zeilen überspringen
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;
        
        // Trim newline
        line[strcspn(line, "\r\n")] = 0;
        
        // Kategorie-Header [EMS], [BAT], etc.
        if (line[0] == '[') {
            if (strstr(line, "[EMS]")) currentCategory = 1;
            else if (strstr(line, "[BAT]")) currentCategory = 2;
            else if (strstr(line, "[PVI]")) currentCategory = 3;
            else if (strstr(line, "[PM]")) currentCategory = 4;
            else if (strstr(line, "[WB]")) currentCategory = 5;
            else if (strstr(line, "[DCDC]")) currentCategory = 6;
            else if (strstr(line, "[INFO]")) currentCategory = 7;
            else if (strstr(line, "[DB]")) currentCategory = 8;
            else if (strstr(line, "[SYS]")) currentCategory = 9;
            else if (strstr(line, "[INTERPRETATIONS]")) currentCategory = 100;
            continue;
        }
        
        if (currentCategory == 100) {
            // Interpretation: 0x01000009:2 = AC-gekoppelt
            char hexStr[32], interp[256];
            if (sscanf(line, "%31[^=] = %255[^\n]", hexStr, interp) == 2) {
                // Trim whitespace
                char* h = hexStr; while (*h == ' ') h++;
                char* hEnd = h + strlen(h) - 1; while (hEnd > h && *hEnd == ' ') *hEnd-- = 0;
                char* i = interp; while (*i == ' ') i++;
                char* iEnd = i + strlen(i) - 1; while (iEnd > i && *iEnd == ' ') *iEnd-- = 0;
                
                loadedInterpretations[std::string(h)] = std::string(i);
            }
        } else if (currentCategory >= 1 && currentCategory <= 9) {
            // Tag: EMS_POWER_PV = 0x01000001 # PV-Leistung in Watt
            char tagName[64], hexStr[32], desc[256];
            char* hashPos = strchr(line, '#');
            
            if (hashPos) {
                *hashPos = '\0';
                hashPos++;
                // Trim description
                while (*hashPos == ' ') hashPos++;
                strncpy(desc, hashPos, sizeof(desc) - 1);
                desc[sizeof(desc) - 1] = '\0';
            } else {
                desc[0] = '\0';
            }
            
            if (sscanf(line, "%63[^=] = %31s", tagName, hexStr) == 2) {
                // Trim whitespace from tagName
                char* t = tagName; while (*t == ' ') t++;
                char* tEnd = t + strlen(t) - 1; while (tEnd > t && *tEnd == ' ') *tEnd-- = 0;
                
                TagInfo info;
                info.name = std::string(t);
                info.hex = (uint32_t)strtoul(hexStr, NULL, 16);
                info.description = std::string(desc);
                
                loadedTags[currentCategory].push_back(info);
            }
        }
    }
    
    fclose(fp);
    DEBUG("Tag-Datei '%s' erfolgreich geladen\n", filename);
}

// Berechnet Tage im Monat (unter Berücksichtigung von Schaltjahren)
int getDaysInMonth(int month, int year) {
    // month: 1-12, year: 4-stellige Jahreszahl
    int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    if (month < 1 || month > 12) return 31;
    
    int days = daysInMonth[month - 1];
    
    // Schaltjahr-Check für Februar
    if (month == 2) {
        // Schaltjahr: Jahr teilbar durch 4, aber nicht durch 100 (außer teilbar durch 400)
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
            days = 29;
        }
    }
    
    return days;
}

// Konvertiert Datum zu Unix-Timestamp mit History-Typ-Anpassung (Anfang der Periode)
time_t dateToTimestamp(const char* dateStr, const char* historyType = "day") {
    struct tm tm_date = {0};
    time_t now;
    int year = 0, month = 0;  // Speichern für Monats-Berechnung
    
    if (strcmp(dateStr, "today") == 0) {
        time(&now);
        struct tm* tm_now = localtime(&now);
        tm_date = *tm_now;
        year = tm_now->tm_year + 1900;
        month = tm_now->tm_mon + 1;
    } else {
        // Parse YYYY-MM-DD Format
        int day;
        if (sscanf(dateStr, "%d-%d-%d", &year, &month, &day) != 3) {
            fprintf(stderr, "Fehler: Ungültiges Datumsformat '%s'\n", dateStr);
            fprintf(stderr, "Verwenden Sie 'today' oder 'YYYY-MM-DD' (z.B. 2024-11-20)\n");
            exit(EXIT_FAILURE);
        }
        
        tm_date.tm_year = year - 1900;  // Jahre seit 1900
        tm_date.tm_mon = month - 1;     // Monat 0-11
        tm_date.tm_mday = day;
        tm_date.tm_isdst = -1;  // Auto-detect DST
    }
    
    // Setze auf Mitternacht (00:00:00)
    tm_date.tm_hour = 0;
    tm_date.tm_min = 0;
    tm_date.tm_sec = 0;
    
    // Anpassung basierend auf History-Typ - BEVOR mktime()
    if (strcmp(historyType, "week") == 0) {
        // Berechne tm_wday erst durch mktime()
        time_t temp = mktime(&tm_date);
        struct tm* pTemp = localtime(&temp);
        if (pTemp) {
            // Gehe zum Montag der aktuellen Woche (tm_wday: 0=Sonntag, 1=Montag)
            int daysToMonday = (pTemp->tm_wday == 0) ? 6 : pTemp->tm_wday - 1;
            tm_date.tm_mday -= daysToMonday;
        }
    } else if (strcmp(historyType, "month") == 0) {
        // Gehe zum 1. des Monats
        tm_date.tm_mday = 1;
        // Speichere die Tage des Monats global für später (wird in createRequestExample verwendet)
        int daysInMonth = getDaysInMonth(month, year);
        g_ctx.historieSpan = daysInMonth * 86400;  // Tage * Sekunden pro Tag
        DEBUG("Monat %d/%d hat %d Tage, SPAN = %u Sekunden\n", month, year, daysInMonth, g_ctx.historieSpan);
    } else if (strcmp(historyType, "year") == 0) {
        // Gehe zum 1. Januar
        tm_date.tm_mon = 0;
        tm_date.tm_mday = 1;
    }
    
    // Jetzt finalen Timestamp berechnen
    time_t timestamp = mktime(&tm_date);
    if (timestamp == -1) {
        fprintf(stderr, "Fehler: Konnte Datum nicht konvertieren\n");
        exit(EXIT_FAILURE);
    }
    
    DEBUG("Konvertiere Datum '%s' (Typ: %s) zu Timestamp: %ld\n", dateStr, historyType, timestamp);
    return timestamp;
}

int createRequestExample(SRscpFrameBuffer * frameBuffer) {
    RscpProtocol protocol;
    SRscpValue rootValue;
    // The root container is create with the TAG ID 0 which is not used by any device.
    protocol.createContainerValue(&rootValue, 0);

    //---------------------------------------------------------------------------------------------------------
    // Create a request frame
    //---------------------------------------------------------------------------------------------------------
    if(iAuthenticated == 0){
        DEBUG("Request authentication\n");
        // authentication request
        SRscpValue authenContainer;
        protocol.createContainerValue(&authenContainer, TAG_RSCP_REQ_AUTHENTICATION);
        protocol.appendValue(&authenContainer, TAG_RSCP_AUTHENTICATION_USER, e3dc_config.e3dc_user);
        protocol.appendValue(&authenContainer, TAG_RSCP_AUTHENTICATION_PASSWORD, e3dc_config.e3dc_password);
        // append sub-container to root container
        protocol.appendValue(&rootValue, authenContainer);
        // free memory of sub-container as it is now copied to rootValue
        protocol.destroyValueData(authenContainer);

    }else{

        if (g_ctx.werteAbfragen){
                DEBUG("Anfrage Tag 0x%08X\n", g_ctx.leseTag);
                
                // Check if this is a BAT_REQ_* tag (0x0300xxxx range) - needs BAT_REQ_DATA container
                if ((g_ctx.leseTag & 0xFF000000) == 0x03000000 && (g_ctx.leseTag & 0x00FF0000) == 0x00000000) {
                    DEBUG("BAT_REQ_* Tag erkannt - erstelle BAT_REQ_DATA Container\n");
                    SRscpValue batContainer;
                    protocol.createContainerValue(&batContainer, TAG_BAT_REQ_DATA);
                    protocol.appendValue(&batContainer, TAG_BAT_INDEX, g_ctx.batIndex);
                    protocol.appendValue(&batContainer, g_ctx.leseTag);
                    protocol.appendValue(&rootValue, batContainer);
                    protocol.destroyValueData(batContainer);
                    g_ctx.batContainerQuery = true;
                } else {
                    protocol.appendValue(&rootValue, g_ctx.leseTag);
                    g_ctx.batContainerQuery = false;
                }
        }
        
        if (g_ctx.modulInfoDump){
                SRscpValue batContainer;
                protocol.createContainerValue(&batContainer, TAG_BAT_REQ_DATA);
                protocol.appendValue(&batContainer, TAG_BAT_INDEX, g_ctx.batIndex);
                
                if (g_ctx.isFirstModuleDumpRequest) {
                    // FIRST REQUEST: Get battery-level data + DCB_COUNT
                    protocol.appendValue(&batContainer, TAG_BAT_REQ_RSOC);           // Relativer SOC
                    protocol.appendValue(&batContainer, TAG_BAT_REQ_ASOC);           // Absoluter SOC / SOH
                    protocol.appendValue(&batContainer, TAG_BAT_REQ_CHARGE_CYCLES);  // Ladezyklen
                    protocol.appendValue(&batContainer, TAG_BAT_REQ_CURRENT);        // Strom
                    protocol.appendValue(&batContainer, TAG_BAT_REQ_MODULE_VOLTAGE); // Modulspannung
                    protocol.appendValue(&batContainer, TAG_BAT_REQ_MAX_BAT_VOLTAGE);// Max. Spannung
                    protocol.appendValue(&batContainer, TAG_BAT_REQ_STATUS_CODE);    // Statuscode
                    protocol.appendValue(&batContainer, TAG_BAT_REQ_ERROR_CODE);     // Fehlercode
                    protocol.appendValue(&batContainer, TAG_BAT_REQ_DCB_COUNT);      // Anzahl DCBs - CRITICAL!
                } else {
                    // SUBSEQUENT REQUESTS: Get specific DCB data
                    protocol.appendValue(&batContainer, TAG_BAT_REQ_DCB_INFO, (uint8_t)g_ctx.currentDCBIndex);
                }
                
                protocol.appendValue(&rootValue, batContainer);
                protocol.destroyValueData(batContainer);
                g_ctx.batContainerQuery = true;
        }
        
        if (g_ctx.historieAbfrage){
                DEBUG("Anfrage Historie: Typ=%s, Datum=%s\n", 
                      g_ctx.historieTyp, g_ctx.historieDatum);
                
                // Determine which history tag to use based on type and set appropriate interval and span
                uint32_t historyTag;
                if (strcmp(g_ctx.historieTyp, "day") == 0) {
                    historyTag = TAG_DB_REQ_HISTORY_DATA_DAY;
                    g_ctx.historieInterval = HISTORY_INTERVAL_DAY;
                    g_ctx.historieSpan = HISTORY_SPAN_DAY;
                } else if (strcmp(g_ctx.historieTyp, "week") == 0) {
                    historyTag = TAG_DB_REQ_HISTORY_DATA_WEEK;
                    g_ctx.historieInterval = HISTORY_INTERVAL_WEEK;
                    g_ctx.historieSpan = HISTORY_SPAN_WEEK;
                } else if (strcmp(g_ctx.historieTyp, "month") == 0) {
                    historyTag = TAG_DB_REQ_HISTORY_DATA_MONTH;
                    g_ctx.historieInterval = HISTORY_INTERVAL_MONTH;
                    // historieSpan wird dynamisch in dateToTimestamp() berechnet!
                } else if (strcmp(g_ctx.historieTyp, "year") == 0) {
                    historyTag = TAG_DB_REQ_HISTORY_DATA_YEAR;
                    g_ctx.historieInterval = HISTORY_INTERVAL_YEAR;
                    g_ctx.historieSpan = HISTORY_SPAN_YEAR;
                } else {
                    fprintf(stderr, "FEHLER: Unbekannter History-Typ: %s\n", g_ctx.historieTyp);
                    exit(EXIT_FAILURE);
                }
                
                // Convert date to timestamp with correct period start
                g_ctx.historieStartTime = dateToTimestamp(g_ctx.historieDatum, g_ctx.historieTyp);
                
                // Create DB_REQ_HISTORY container
                SRscpValue historyContainer;
                protocol.createContainerValue(&historyContainer, historyTag);
                protocol.appendValue(&historyContainer, TAG_DB_REQ_HISTORY_TIME_START, (uint64_t)g_ctx.historieStartTime);
                protocol.appendValue(&historyContainer, TAG_DB_REQ_HISTORY_TIME_INTERVAL, g_ctx.historieInterval);
                protocol.appendValue(&historyContainer, TAG_DB_REQ_HISTORY_TIME_SPAN, g_ctx.historieSpan);
                
                // Append history container to root
                protocol.appendValue(&rootValue, historyContainer);
                protocol.destroyValueData(historyContainer);
        }

        if (g_ctx.manuelleSpeicherladung){
                protocol.appendValue(&rootValue, TAG_EMS_REQ_START_MANUAL_CHARGE, g_ctx.ladungsMenge);
        }

        if (g_ctx.leistungAendern){

                SRscpValue PMContainer;
                protocol.createContainerValue(&PMContainer, TAG_EMS_REQ_SET_POWER_SETTINGS);

            if (g_ctx.automatischLeistungEinstellen){

              printf("Setze Lade-/EntladeLeistung auf Automatik\n");
              protocol.appendValue(&PMContainer, TAG_EMS_POWER_LIMITS_USED, false);

            }

            if (g_ctx.ladeLeistungGesetzt || g_ctx.entladeLeistungGesetzt){

              protocol.appendValue(&PMContainer, TAG_EMS_POWER_LIMITS_USED, true);

              if (g_ctx.ladeLeistungGesetzt){

                printf("Setze LadeLeistung auf %iW\n",g_ctx.ladeLeistung);
                protocol.appendValue(&PMContainer, TAG_EMS_MAX_CHARGE_POWER, g_ctx.ladeLeistung);

              }

              if (g_ctx.entladeLeistungGesetzt){

                printf("Setze EntladeLeistung auf %iW\n",g_ctx.entladeLeistung);
                protocol.appendValue(&PMContainer, TAG_EMS_MAX_DISCHARGE_POWER, g_ctx.entladeLeistung);

              }

            }

                // append sub-container to root container
            protocol.appendValue(&rootValue, PMContainer);
            // free memory of sub-container as it is now copied to rootValue
            protocol.destroyValueData(PMContainer);

        }

    }

    // create buffer frame to send data to the S10
    protocol.createFrameAsBuffer(frameBuffer, rootValue.data, rootValue.length, true); // true to calculate CRC on for transfer
    // the root value object should be destroyed after the data is copied into the frameBuffer and is not needed anymore
    protocol.destroyValueData(rootValue);

    return 0;
}

// Get tag description from loaded tags (search all categories)
const char* getTagDescription(uint32_t tag) {
    // For RESPONSE tags (0x??8?????), try to find the REQUEST tag description first
    uint32_t requestTag = tag;
    if ((tag & 0x00800000) != 0) {
        requestTag = tag & ~0x00800000;  // Clear response bit
    }
    
    // Search through all categories for REQUEST tag first
    for (auto& categoryPair : loadedTags) {
        for (auto& tagInfo : categoryPair.second) {
            if (tagInfo.hex == requestTag) {
                return tagInfo.description.c_str();
            }
        }
    }
    
    // If not found and this was a RESPONSE tag, try original tag
    if (requestTag != tag) {
        for (auto& categoryPair : loadedTags) {
            for (auto& tagInfo : categoryPair.second) {
                if (tagInfo.hex == tag) {
                    return tagInfo.description.c_str();
                }
            }
        }
    }
    
    return NULL;
}

// Format millisecond Unix epoch timestamp to human-readable string
std::string formatTimestamp(uint64_t milliseconds) {
    time_t seconds = milliseconds / 1000;
    struct tm timeinfo;
    localtime_r(&seconds, &timeinfo);
    
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    return std::string(buffer);
}

const char* interpretValue(uint32_t tag, int64_t value) {
    // Suche Interpretation in geladenen Daten aus e3dcset.tags
    char key[64];
    snprintf(key, sizeof(key), "0x%08X:%lld", tag, (long long)value);
    auto it = loadedInterpretations.find(std::string(key));
    if (it != loadedInterpretations.end()) {
        return it->second.c_str();
    }
    
    // Falls RESPONSE-Tag: Versuche mit REQUEST-Tag (zweites Byte & 0x7F)
    uint8_t secondByte = (tag >> 16) & 0xFF;
    if (secondByte >= 0x80) {
        // Konvertiere RESPONSE zu REQUEST: zweites Byte AND 0x7F
        uint32_t requestTag = (tag & 0xFF00FFFF) | (((secondByte & 0x7F) << 16));
        snprintf(key, sizeof(key), "0x%08X:%lld", requestTag, (long long)value);
        it = loadedInterpretations.find(std::string(key));
        if (it != loadedInterpretations.end()) {
            return it->second.c_str();
        }
    }
    
    // Keine Interpretation verfügbar
    return NULL;
}

// Unified value formatter - eliminiert Code-Duplikation in Response-Handling
void printFormattedValue(uint32_t tag, const char* valueStr, int64_t numericValue) {
    if (g_ctx.quietMode) {
        printf("%s\n", valueStr);
    } else {
        const char* interp = interpretValue(tag, numericValue);
        if (interp) {
            printf("%s (%s)\n", valueStr, interp);
        } else {
            printf("%s\n", valueStr);
        }
    }
}

int handleResponseValue(RscpProtocol *protocol, SRscpValue *response) {
    // check if any of the response has the error flag set and react accordingly
    if(response->dataType == RSCP::eTypeError) {
        // handle error for example access denied errors
        uint32_t uiErrorCode = protocol->getValueAsUInt32(response);
        printf("Tag 0x%08X received error code %u.\n", response->tag, uiErrorCode);
        return -1;
    }

    // check the SRscpValue TAG to detect which response it is
    switch(response->tag){
    case TAG_RSCP_AUTHENTICATION: {
        // It is possible to check the response->dataType value to detect correct data type
        // and call the correct function. If data type is known,
        // the correct function can be called directly like in this case.
        uint8_t ucAccessLevel = protocol->getValueAsUChar8(response);
        if(ucAccessLevel > 0) {
            iAuthenticated = 1;
        }
        DEBUG("RSCP authentitication level %i\n", ucAccessLevel);
        break;
    }
    case TAG_EMS_START_MANUAL_CHARGE: {

        if (protocol->getValueAsBool(response)){

                if (g_ctx.ladungsMenge == 0){
                        printf("Manuelles Laden gestoppt\n");
                }else{
                        printf("Manuelles Laden gestartet\n");
                }

        }else{
          printf("Manuelles Laden abgeleht.\n");
        }
        break;
    } 
    case TAG_EMS_POWER_PV: {    // response for TAG_EMS_REQ_POWER_PV
        int32_t iPower = protocol->getValueAsInt32(response);
        if (g_ctx.quietMode) {
            printf("%i\n", iPower);
        } else {
            printf("EMS PV power is %i W\n", iPower);
        }
        break;
    }
    case TAG_EMS_POWER_BAT: {    // response for TAG_EMS_REQ_POWER_BAT
        int32_t iPower = protocol->getValueAsInt32(response);
        if (g_ctx.quietMode) {
            printf("%i\n", iPower);
        } else {
            printf("EMS BAT power is %i W\n", iPower);
        }
        break;
    }
    case TAG_EMS_POWER_HOME: {    // response for TAG_EMS_REQ_POWER_HOME
        int32_t iPower = protocol->getValueAsInt32(response);
        if (g_ctx.quietMode) {
            printf("%i\n", iPower);
        } else {
            printf("EMS house power is %i W\n", iPower);
        }
        break;
    }
    case TAG_EMS_POWER_GRID: {    // response for TAG_EMS_REQ_POWER_GRID
        int32_t iPower = protocol->getValueAsInt32(response);
        if (g_ctx.quietMode) {
            printf("%i\n", iPower);
        } else {
            printf("EMS grid power is %i W\n", iPower);
        }
        break;
    }
    case TAG_EMS_POWER_ADD: {    // response for TAG_EMS_REQ_POWER_ADD
        int32_t iPower = protocol->getValueAsInt32(response);
        if (g_ctx.quietMode) {
            printf("%i\n", iPower);
        } else {
            printf("EMS add power meter power is %i W\n", iPower);
        }
        break;
    }
    case TAG_BAT_DATA: {        // response for TAG_BAT_REQ_DATA
        std::vector<SRscpValue> batteryData = protocol->getValueAsContainer(response);
        
        // Calculate expected response tag from request tag (REQUEST 0x03xxxx -> RESPONSE 0x38xxxx)
        uint32_t expectedResponseTag = 0;
        if (g_ctx.batContainerQuery && !g_ctx.modulInfoDump) {
            expectedResponseTag = g_ctx.leseTag | 0x00800000;  // Set bit 23 (0x00800000) for RESPONSE
        }
        
        bool foundRequestedTag = false;
        bool receivedDCBData = false;  // Track if this response contained actual DCB data
        
        // Print header for module info dump
        if (g_ctx.modulInfoDump && !g_ctx.quietMode) {
            printf("Batterie Modul %u:\n", g_ctx.batIndex);
        }
        
        for(size_t i = 0; i < batteryData.size(); ++i) {
            // Check for errors first - stop processing if error found
            if(batteryData[i].dataType == RSCP::eTypeError) {
                uint32_t uiErrorCode = protocol->getValueAsUInt32(&batteryData[i]);
                // Always output errors to stderr (quiet-mode contract)
                fprintf(stderr, "Fehler: Tag 0x%08X, Code %u\n", batteryData[i].tag, uiErrorCode);
                // Clean up vector elements
                for(size_t j = 0; j < batteryData.size(); ++j) {
                    protocol->destroyValueData(&batteryData[j]);
                }
                g_ctx.batContainerQuery = false;  // Reset flag
                return -1;  // Stop processing after error
            }
            
            // Skip BAT_INDEX in output
            if (batteryData[i].tag == TAG_BAT_INDEX) {
                continue;
            }
            
            // Special handling for DCB_COUNT in module dump mode
            if (batteryData[i].tag == TAG_BAT_DCB_COUNT && g_ctx.modulInfoDump) {
                uint8_t dcbCount = protocol->getValueAsUChar8(&batteryData[i]);
                g_ctx.totalDCBs = dcbCount;
                
                // If we have DCBs and this is the first request, set up the multi-request loop
                if (dcbCount > 0 && g_ctx.isFirstModuleDumpRequest) {
                    g_ctx.needMoreDCBRequests = true;
                    g_ctx.currentDCBIndex = 0;
                    g_ctx.isFirstModuleDumpRequest = false;
                } else if (dcbCount == 0) {
                    // No DCBs - reset state
                    g_ctx.needMoreDCBRequests = false;
                    g_ctx.isFirstModuleDumpRequest = true;
                }
                // Continue to print DCB_COUNT in output
            }
            
            // In quiet mode (single tag query), only process the requested tag's value
            if (g_ctx.quietMode && !g_ctx.modulInfoDump && batteryData[i].tag != expectedResponseTag) {
                continue;
            }
            
            // Mark that we found the requested tag
            if (batteryData[i].tag == expectedResponseTag) {
                foundRequestedTag = true;
            }
            
            // Print tag prefix - formatted for module dump, raw for single query
            if (!g_ctx.quietMode) {
                if (g_ctx.modulInfoDump) {
                    // Friendly label for module info dump
                    const char* label = getTagDescription(batteryData[i].tag);
                    if (label) {
                        printf("  %-30s ", label);
                    } else {
                        printf("  Tag 0x%08X:                  ", batteryData[i].tag);
                    }
                } else {
                    printf("Tag 0x%08X: ", batteryData[i].tag);
                }
            }
            
            // Process battery value based on datatype - uses printFormattedValue for interpretations
            switch(batteryData[i].dataType) {
                case RSCP::eTypeFloat32: {
                    float value = protocol->getValueAsFloat32(&batteryData[i]);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.2f", value);
                    // Use std::llround for proper rounding (handles negative values)
                    int64_t roundedValue = std::llround(value);
                    const char* interp = interpretValue(batteryData[i].tag, roundedValue);
                    if (g_ctx.quietMode) {
                        printf("%s\n", buf);
                    } else if (interp) {
                        printf("%s (%s)\n", buf, interp);
                    } else {
                        printf("%s\n", buf);
                    }
                    break;
                }
                case RSCP::eTypeUChar8: {
                    uint8_t value = protocol->getValueAsUChar8(&batteryData[i]);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%u", value);
                    printFormattedValue(batteryData[i].tag, buf, value);
                    break;
                }
                case RSCP::eTypeInt32: {
                    int32_t value = protocol->getValueAsInt32(&batteryData[i]);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%d", value);
                    printFormattedValue(batteryData[i].tag, buf, value);
                    break;
                }
                case RSCP::eTypeUInt32: {
                    uint32_t value = protocol->getValueAsUInt32(&batteryData[i]);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%u", value);
                    printFormattedValue(batteryData[i].tag, buf, value);
                    break;
                }
                case RSCP::eTypeString: {
                    std::string str = protocol->getValueAsString(&batteryData[i]);
                    printf("%s\n", str.c_str());
                    break;
                }
                case RSCP::eTypeContainer: {
                    // Handle nested containers - especially TAG_BAT_DCB_INFO
                    if (batteryData[i].tag == TAG_BAT_DCB_INFO && g_ctx.modulInfoDump) {
                        std::vector<SRscpValue> dcbInfoData = protocol->getValueAsContainer(&batteryData[i]);
                        
                        // Group DCB data by DCB_INDEX (ALWAYS parse, regardless of quiet mode)
                        std::map<uint8_t, std::vector<std::pair<uint32_t, SRscpValue>>> dcbData;
                        int8_t currentDcbIndex = -1;
                        
                        for(size_t j = 0; j < dcbInfoData.size(); ++j) {
                            uint32_t tag = dcbInfoData[j].tag;
                            
                            if (tag == TAG_BAT_DCB_INDEX) {
                                currentDcbIndex = protocol->getValueAsUChar8(&dcbInfoData[j]);
                                receivedDCBData = true;  // CRITICAL: Set flag regardless of output mode!
                            } else if (currentDcbIndex >= 0) {
                                // Check if this is a DCB-related tag
                                if ((tag & 0xFFF00000) == 0x03800000) {
                                    dcbData[currentDcbIndex].push_back(std::make_pair(tag, dcbInfoData[j]));
                                }
                            }
                        }
                        
                        // Print grouped DCB data (only if NOT in quiet mode)
                        if (!g_ctx.quietMode && dcbData.size() > 0) {
                            printf("\n\n  === DCB Zellblöcke ===\n");
                            for (auto& dcbPair : dcbData) {
                                printf("  Zellblock %u:\n", dcbPair.first);
                                
                                for (auto& tagValuePair : dcbPair.second) {
                                    const char* label = getTagDescription(tagValuePair.first);
                                    if (label) {
                                        printf("    %s\n", label);
                                    } else {
                                        printf("    Tag 0x%08X:\n", tagValuePair.first);
                                    }
                                    
                                    // Formatiere Wert mit 2 Leerzeichen Einrückung
                                    switch(tagValuePair.second.dataType) {
                                        case RSCP::eTypeBool:
                                            printf("  %s\n", protocol->getValueAsBool(&tagValuePair.second) ? "true" : "false");
                                            break;
                                        case RSCP::eTypeChar8:
                                            printf("  %d\n", protocol->getValueAsChar8(&tagValuePair.second));
                                            break;
                                        case RSCP::eTypeUChar8:
                                            printf("  %u\n", protocol->getValueAsUChar8(&tagValuePair.second));
                                            break;
                                        case RSCP::eTypeInt16:
                                            printf("  %d\n", protocol->getValueAsInt16(&tagValuePair.second));
                                            break;
                                        case RSCP::eTypeUInt16:
                                            printf("  %u\n", protocol->getValueAsUInt16(&tagValuePair.second));
                                            break;
                                        case RSCP::eTypeInt32:
                                            printf("  %d\n", protocol->getValueAsInt32(&tagValuePair.second));
                                            break;
                                        case RSCP::eTypeUInt32:
                                            printf("  %u\n", protocol->getValueAsUInt32(&tagValuePair.second));
                                            break;
                                        case RSCP::eTypeInt64:
                                            printf("  %lld\n", (long long)protocol->getValueAsInt64(&tagValuePair.second));
                                            break;
                                        case RSCP::eTypeUInt64: {
                                            uint64_t value = protocol->getValueAsUInt64(&tagValuePair.second);
                                            // Special formatting for timestamp tags
                                            if (tagValuePair.first == TAG_BAT_DCB_LAST_MESSAGE_TIMESTAMP) {
                                                std::string formatted = formatTimestamp(value);
                                                printf("  %s\n", formatted.c_str());
                                            } else {
                                                printf("  %llu\n", (unsigned long long)value);
                                            }
                                            break;
                                        }
                                        case RSCP::eTypeFloat32:
                                            printf("  %.2f\n", protocol->getValueAsFloat32(&tagValuePair.second));
                                            break;
                                        case RSCP::eTypeDouble64:
                                            printf("  %.4f\n", protocol->getValueAsDouble64(&tagValuePair.second));
                                            break;
                                        case RSCP::eTypeString: {
                                            std::string str = protocol->getValueAsString(&tagValuePair.second);
                                            if (str.empty()) {
                                                printf("  (leer)\n");
                                            } else {
                                                printf("  %s\n", str.c_str());
                                            }
                                            break;
                                        }
                                        case RSCP::eTypeBitfield: {
                                            // Bitfield als Hex ausgeben
                                            uint32_t bitfield = 0;
                                            if (tagValuePair.second.length == 1) {
                                                bitfield = protocol->getValueAsUChar8(&tagValuePair.second);
                                            } else if (tagValuePair.second.length == 2) {
                                                bitfield = protocol->getValueAsUInt16(&tagValuePair.second);
                                            } else if (tagValuePair.second.length == 4) {
                                                bitfield = protocol->getValueAsUInt32(&tagValuePair.second);
                                            }
                                            printf("  0x%0*X\n", tagValuePair.second.length * 2, bitfield);
                                            break;
                                        }
                                        case RSCP::eTypeByteArray: {
                                            // ByteArray als Hex ausgeben
                                            printf("  0x");
                                            for (uint16_t k = 0; k < tagValuePair.second.length; k++) {
                                                printf("%02X", tagValuePair.second.data[k]);
                                            }
                                            printf("\n");
                                            break;
                                        }
                                        default:
                                            printf("  (Typ %d)\n", tagValuePair.second.dataType);
                                            break;
                                    }
                                }
                                printf("\n");
                            }
                        }
                        
                        // Clean up
                        for(size_t j = 0; j < dcbInfoData.size(); ++j) {
                            protocol->destroyValueData(&dcbInfoData[j]);
                        }
                    } else if (!g_ctx.quietMode) {
                        printf("(Container mit %zu Elementen)\n", 
                               protocol->getValueAsContainer(&batteryData[i]).size());
                    }
                    break;
                }
                default:
                    if (!g_ctx.quietMode) {
                        printf("Unbekannter Datentyp %d\n", batteryData[i].dataType);
                    }
                    break;
            }
            
            // In quiet mode (single tag query), stop after printing the requested value
            if (g_ctx.quietMode && !g_ctx.modulInfoDump && foundRequestedTag) {
                break;
            }
        }
        
        // In quiet mode (single tag query), if we didn't find the requested tag, output error
        if (g_ctx.quietMode && !g_ctx.modulInfoDump && !foundRequestedTag) {
            fprintf(stderr, "Fehler: Angeforderter Tag 0x%08X nicht in Response gefunden\n", expectedResponseTag);
        }
        
        // CRITICAL: Multi-DCB Loop Management
        // Only increment if we actually received DCB data (not just battery-level response)
        if (g_ctx.needMoreDCBRequests && g_ctx.modulInfoDump && receivedDCBData) {
            g_ctx.currentDCBIndex++;
            
            // Check if we've queried all DCBs
            if (g_ctx.currentDCBIndex >= g_ctx.totalDCBs) {
                g_ctx.needMoreDCBRequests = false;
                g_ctx.isFirstModuleDumpRequest = true;  // Reset for next dump
            }
        }
        
        // Clean up vector elements properly
        for(size_t i = 0; i < batteryData.size(); ++i) {
            protocol->destroyValueData(&batteryData[i]);
        }
        
        g_ctx.batContainerQuery = false;  // Reset flag after successful processing
        break;
       }
       
        case TAG_BAT_DCB_INFO: {        // response for TAG_BAT_REQ_DCB_INFO
            if (!g_ctx.modulInfoDump || g_ctx.quietMode) {
                break;  // Only process in module info dump mode
            }
            
            // TAG_BAT_DCB_INFO can be either a single container (1 DCB) or a container of containers (multiple DCBs)
            // We need to handle both cases
            
            // First, check if this is a container of containers or just a single DCB container
            std::vector<SRscpValue> dcbInfoData = protocol->getValueAsContainer(response);
            
            // Check if the first element is BAT_DCB_INDEX (single DCB) or another container (multiple DCBs)
            if (dcbInfoData.size() > 0) {
                // If first element is BAT_DCB_INDEX, it's a single DCB container
                // Otherwise, it might be multiple containers
                
                bool isSingleDCB = (dcbInfoData[0].tag == TAG_BAT_DCB_INDEX);
                
                if (isSingleDCB) {
                    // Single DCB: process directly - reuse the same logic as multiple DCBs
                    std::map<uint8_t, std::vector<std::pair<uint32_t, SRscpValue>>> dcbData;
                    int8_t currentDcbIndex = -1;
                    
                    for(size_t i = 0; i < dcbInfoData.size(); ++i) {
                        uint32_t tag = dcbInfoData[i].tag;
                        
                        if (tag == TAG_BAT_DCB_INDEX) {
                            currentDcbIndex = protocol->getValueAsUChar8(&dcbInfoData[i]);
                        } else if (currentDcbIndex >= 0) {
                            if ((tag & 0xFFF00000) == 0x03800000) {
                                dcbData[currentDcbIndex].push_back(std::make_pair(tag, dcbInfoData[i]));
                            }
                        }
                    }
                    
                    // Print the single DCB
                    if (!g_ctx.quietMode && dcbData.size() > 0) {
                        printf("\n  === DCB Zellblöcke ===\n");
                        for (auto& dcbPair : dcbData) {
                            printf("  Zellblock %u:\n", dcbPair.first);
                            
                            for (auto& tagValuePair : dcbPair.second) {
                                const char* label = getTagDescription(tagValuePair.first);
                                if (label) {
                                    printf("    %-35s ", label);
                                } else {
                                    printf("    Tag 0x%08X:                     ", tagValuePair.first);
                                }
                                
                                switch(tagValuePair.second.dataType) {
                                    case RSCP::eTypeFloat32:
                                        printf("%.2f\n", protocol->getValueAsFloat32(&tagValuePair.second));
                                        break;
                                    case RSCP::eTypeUChar8:
                                        printf("%u\n", protocol->getValueAsUChar8(&tagValuePair.second));
                                        break;
                                    case RSCP::eTypeUInt32:
                                        printf("%u\n", protocol->getValueAsUInt32(&tagValuePair.second));
                                        break;
                                    case RSCP::eTypeInt32:
                                        printf("%d\n", protocol->getValueAsInt32(&tagValuePair.second));
                                        break;
                                    default:
                                        printf("(Typ %d)\n", tagValuePair.second.dataType);
                                        break;
                                }
                            }
                            printf("\n");
                        }
                    }
                } else {
                    // Multiple DCBs: each element might be a container
                    // Group data by DCB_INDEX
                    std::map<uint8_t, std::vector<std::pair<uint32_t, SRscpValue>>> dcbData;
                    int8_t currentDcbIndex = -1;
                    
                    for(size_t i = 0; i < dcbInfoData.size(); ++i) {
                        uint32_t tag = dcbInfoData[i].tag;
                        
                        if (tag == TAG_BAT_DCB_INDEX) {
                            currentDcbIndex = protocol->getValueAsUChar8(&dcbInfoData[i]);
                            DEBUG("Gefundener DCB_INDEX: %d\n", currentDcbIndex);
                        } else if (currentDcbIndex >= 0) {
                            // Check if this is a DCB-related tag
                            if ((tag & 0xFFF00000) == 0x03800000) {
                                dcbData[currentDcbIndex].push_back(std::make_pair(tag, dcbInfoData[i]));
                                DEBUG("  Tag 0x%08X zugeordnet zu DCB %d\n", tag, currentDcbIndex);
                            }
                        }
                    }
                    
                    // Print grouped DCB data
                    if (!g_ctx.quietMode && dcbData.size() > 0) {
                        printf("\n  === DCB Zellblöcke ===\n");
                        for (auto& dcbPair : dcbData) {
                            printf("  Zellblock %u:\n", dcbPair.first);
                            
                            for (auto& tagValuePair : dcbPair.second) {
                                const char* label = getTagDescription(tagValuePair.first);
                                if (label) {
                                    printf("    %-35s ", label);
                                } else {
                                    printf("    Tag 0x%08X:                     ", tagValuePair.first);
                                }
                                
                                // Print value based on type
                                switch(tagValuePair.second.dataType) {
                                    case RSCP::eTypeFloat32: {
                                        float val = protocol->getValueAsFloat32(&tagValuePair.second);
                                        printf("%.2f\n", val);
                                        break;
                                    }
                                    case RSCP::eTypeUChar8: {
                                        uint8_t val = protocol->getValueAsUChar8(&tagValuePair.second);
                                        printf("%u\n", val);
                                        break;
                                    }
                                    case RSCP::eTypeUInt32: {
                                        uint32_t val = protocol->getValueAsUInt32(&tagValuePair.second);
                                        printf("%u\n", val);
                                        break;
                                    }
                                    case RSCP::eTypeInt32: {
                                        int32_t val = protocol->getValueAsInt32(&tagValuePair.second);
                                        printf("%d\n", val);
                                        break;
                                    }
                                    default:
                                        printf("(Typ %d)\n", tagValuePair.second.dataType);
                                        break;
                                }
                            }
                            printf("\n");
                        }
                    }
                }
            }
            
            // Clean up
            for(size_t i = 0; i < dcbInfoData.size(); ++i) {
                protocol->destroyValueData(&dcbInfoData[i]);
            }
            
            // After processing DCB response, check if we need more DCB requests
            if (g_ctx.needMoreDCBRequests) {
                g_ctx.currentDCBIndex++;
                DEBUG("DCB #%u verarbeitet, nächster Index: %u von %u\n", 
                      g_ctx.currentDCBIndex - 1, g_ctx.currentDCBIndex, g_ctx.totalDCBs);
                
                // Check if we've queried all DCBs
                if (g_ctx.currentDCBIndex >= g_ctx.totalDCBs) {
                    g_ctx.needMoreDCBRequests = false;
                    g_ctx.isFirstModuleDumpRequest = true;  // Reset for next dump
                    DEBUG("Alle %u DCBs abgefragt - Multi-Request-Loop beendet\n", g_ctx.totalDCBs);
                }
            }
            
            break;
        }

        case TAG_EMS_SET_POWER_SETTINGS: {        // response for TAG_PM_REQ_DATA
            uint8_t ucPMIndex = 0;
            std::vector<SRscpValue> PMData = protocol->getValueAsContainer(response);
            for(size_t i = 0; i < PMData.size(); ++i) {
                if(PMData[i].dataType == RSCP::eTypeError) {
                    // handle error for example access denied errors
                    uint32_t uiErrorCode = protocol->getValueAsUInt32(&PMData[i]);
                    printf("TAG_EMS_GET_POWER_SETTINGS 0x%08X received error code %u.\n", PMData[i].tag, uiErrorCode);
                    return -1;
                }
                // check each PM sub tag
                switch(PMData[i].tag) {
                    case TAG_PM_INDEX: {
                        ucPMIndex = protocol->getValueAsUChar8(&PMData[i]);
                        break;
                    }
                    case TAG_EMS_POWER_LIMITS_USED: {              // response for POWER_LIMITS_USED
                        if (protocol->getValueAsBool(&PMData[i])){
                            printf("POWER_LIMITS_USED\n");
                            }
                        break;
                    }
                    case TAG_EMS_MAX_CHARGE_POWER: {              // 101 response for TAG_EMS_MAX_CHARGE_POWER
                        uint32_t uPower = protocol->getValueAsUInt32(&PMData[i]);
                        printf("MAX_CHARGE_POWER %i W\n", uPower);
                        break;
                    }
                    case TAG_EMS_MAX_DISCHARGE_POWER: {              //102 response for TAG_EMS_MAX_DISCHARGE_POWER
                        uint32_t uPower = protocol->getValueAsUInt32(&PMData[i]);
                        printf("MAX_DISCHARGE_POWER %i W\n", uPower);
                        break;
                    }
                    case TAG_EMS_DISCHARGE_START_POWER:{              //103 response for TAG_EMS_DISCHARGE_START_POWER
                        uint32_t uPower = protocol->getValueAsUInt32(&PMData[i]);
                        printf("DISCHARGE_START_POWER %i W\n", uPower);
                        break;
                    }
                    case TAG_EMS_POWERSAVE_ENABLED: {              //104 response for TAG_EMS_POWERSAVE_ENABLED
                        if (protocol->getValueAsBool(&PMData[i])){
                            printf("POWERSAVE_ENABLED\n");
                        }
                        break;
                    }
                    case TAG_EMS_WEATHER_REGULATED_CHARGE_ENABLED: {//105 resp WEATHER_REGULATED_CHARGE_ENABLED
                        if (protocol->getValueAsBool(&PMData[i])){
                            printf("WEATHER_REGULATED_CHARGE_ENABLED\n");
                        }
                        break;
                    }
                        // ...
                    default:
                        // default behaviour
                        break;
                }
            }
            protocol->destroyValueData(PMData);
            break;

    }
    
    // History data responses
    case TAG_DB_HISTORY_DATA_DAY:
    case TAG_DB_HISTORY_DATA_WEEK:
    case TAG_DB_HISTORY_DATA_MONTH:
    case TAG_DB_HISTORY_DATA_YEAR: {
        if (!g_ctx.historieAbfrage) {
            printf("Unerwartete History-Response (Tag 0x%08X)\n", response->tag);
            break;
        }
        
        const char* typeStr = "Unknown";
        const char* intervalName = "";
        const char* spanName = "";
        if (response->tag == TAG_DB_HISTORY_DATA_DAY) {
            typeStr = "Tag";
            spanName = "24 Stunden";
            intervalName = "15 Minuten";
        } else if (response->tag == TAG_DB_HISTORY_DATA_WEEK) {
            typeStr = "Woche";
            spanName = "7 Tage";
            intervalName = "1 Stunde";
        } else if (response->tag == TAG_DB_HISTORY_DATA_MONTH) {
            typeStr = "Monat";
            spanName = "30 Tage";
            intervalName = "1 Tag";
        } else if (response->tag == TAG_DB_HISTORY_DATA_YEAR) {
            typeStr = "Jahr";
            spanName = "365 Tage";
            intervalName = "1 Woche";
        }
        
        // Format start and end dates
        time_t startTime = g_ctx.historieStartTime;
        time_t endTime = g_ctx.historieStartTime + g_ctx.historieSpan - 1;
        
        char startStr[32] = "N/A", endStr[32] = "N/A";
        
        if (startTime > 0) {
            struct tm startTm, endTm;
            
            // WICHTIG: localtime() überschreibt statischen Buffer!
            // Deshalb SOFORT nach jedem Aufruf kopieren!
            struct tm *pStartTm = localtime(&startTime);
            if (pStartTm) {
                startTm = *pStartTm;  // Sofort kopieren!
                strftime(startStr, sizeof(startStr), "%d.%m.%Y", &startTm);
            }
            
            struct tm *pEndTm = localtime(&endTime);
            if (pEndTm) {
                endTm = *pEndTm;  // Sofort kopieren!
                strftime(endStr, sizeof(endStr), "%d.%m.%Y", &endTm);
            }
        }
        
        printf("Zeitraum: %s - %s\n", startStr, endStr);
        
        std::vector<SRscpValue> historyData = protocol->getValueAsContainer(response);
        
        for(size_t i = 0; i < historyData.size(); ++i) {
            if(historyData[i].dataType == RSCP::eTypeError) {
                uint32_t uiErrorCode = protocol->getValueAsUInt32(&historyData[i]);
                printf("Fehler: Tag 0x%08X, Code %u\n", historyData[i].tag, uiErrorCode);
                continue;
            }
            
            switch(historyData[i].tag) {
                case TAG_DB_SUM_CONTAINER: {
                    std::vector<SRscpValue> sumData = protocol->getValueAsContainer(&historyData[i]);
                    
                    float batPowerIn = 0, batPowerOut = 0, dcPower = 0;
                    float gridPowerIn = 0, gridPowerOut = 0, consumption = 0;
                    float soc = 0, autarky = 0;
                    uint32_t graphIndex = 0;
                    
                    for(size_t j = 0; j < sumData.size(); ++j) {
                        switch(sumData[j].tag) {
                            case TAG_DB_GRAPH_INDEX:
                                graphIndex = protocol->getValueAsUInt32(&sumData[j]);
                                break;
                            case TAG_DB_BAT_POWER_IN:
                                batPowerIn = protocol->getValueAsFloat32(&sumData[j]);
                                break;
                            case TAG_DB_BAT_POWER_OUT:
                                batPowerOut = protocol->getValueAsFloat32(&sumData[j]);
                                break;
                            case TAG_DB_DC_POWER:
                                dcPower = protocol->getValueAsFloat32(&sumData[j]);
                                break;
                            case TAG_DB_GRID_POWER_IN:
                                gridPowerIn = protocol->getValueAsFloat32(&sumData[j]);
                                break;
                            case TAG_DB_GRID_POWER_OUT:
                                gridPowerOut = protocol->getValueAsFloat32(&sumData[j]);
                                break;
                            case TAG_DB_CONSUMPTION:
                                consumption = protocol->getValueAsFloat32(&sumData[j]);
                                break;
                            case TAG_DB_BAT_CHARGE_LEVEL:
                                soc = protocol->getValueAsFloat32(&sumData[j]);
                                break;
                            case TAG_DB_AUTARKY:
                                autarky = protocol->getValueAsFloat32(&sumData[j]);
                                break;
                            case TAG_DB_BAT_CYCLE_COUNT:
                            case TAG_DB_CONSUMED_PRODUCTION:
                            case TAG_DB_PM_0_POWER:
                            case TAG_DB_PM_1_POWER:
                                // Diese Tags werden aktuell nicht ausgegeben
                                break;
                        }
                    }
                    
                    printf("PV-Produktion:      %.2f kWh\n", dcPower / 1000.0);
                    printf("Batterie geladen:   %.2f kWh\n", batPowerIn / 1000.0);
                    printf("Batterie entladen:  %.2f kWh\n", batPowerOut / 1000.0);
                    printf("Netzbezug:          %.2f kWh\n", gridPowerOut / 1000.0);
                    printf("Netzeinspeisung:    %.2f kWh\n", gridPowerIn / 1000.0);
                    printf("Hausverbrauch:      %.2f kWh\n", consumption / 1000.0);
                    if (autarky > 0) printf("Autarkie:           %.1f %%\n", autarky);
                    
                    protocol->destroyValueData(sumData);
                    break;
                }
                case TAG_DB_VALUE_CONTAINER: {
                    // Datenpunkte werden nicht angezeigt - nur Zusammenfassung
                    std::vector<SRscpValue> tmpData = protocol->getValueAsContainer(&historyData[i]);
                    protocol->destroyValueData(tmpData);
                    break;
                }
                default:
                    printf("  Unbekannter History-Sub-Tag 0x%08X\n", historyData[i].tag);
                    break;
            }
        }
        
        protocol->destroyValueData(historyData);
        break;
    }
    
    // ...
    default:
        // Generic handler for read requests
        if (g_ctx.werteAbfragen) {
            if (!g_ctx.quietMode) {
                printf("Tag 0x%08X: ", response->tag);
            }
            switch(response->dataType) {
                case RSCP::eTypeBool: {
                    bool bValue = protocol->getValueAsBool(response);
                    printFormattedValue(response->tag, bValue ? "true" : "false", bValue ? 1 : 0);
                    break;
                }
                case RSCP::eTypeChar8: {
                    int8_t value = protocol->getValueAsChar8(response);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%d", value);
                    printFormattedValue(response->tag, buf, value);
                    break;
                }
                case RSCP::eTypeUChar8: {
                    uint8_t value = protocol->getValueAsUChar8(response);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%u", value);
                    printFormattedValue(response->tag, buf, value);
                    break;
                }
                case RSCP::eTypeInt16: {
                    int16_t value = protocol->getValueAsInt16(response);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%d", value);
                    printFormattedValue(response->tag, buf, value);
                    break;
                }
                case RSCP::eTypeUInt16: {
                    uint16_t value = protocol->getValueAsUInt16(response);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%u", value);
                    printFormattedValue(response->tag, buf, value);
                    break;
                }
                case RSCP::eTypeInt32: {
                    int32_t value = protocol->getValueAsInt32(response);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%d", value);
                    printFormattedValue(response->tag, buf, value);
                    break;
                }
                case RSCP::eTypeUInt32: {
                    uint32_t value = protocol->getValueAsUInt32(response);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%u", value);
                    printFormattedValue(response->tag, buf, value);
                    break;
                }
                case RSCP::eTypeInt64: {
                    long long value = (long long)protocol->getValueAsInt64(response);
                    // Check if value looks like a millisecond timestamp (between 2020-2040)
                    if (!g_ctx.quietMode && value > 1577836800000LL && value < 2209075200000LL) {
                        time_t seconds = (time_t)(value / 1000);
                        int milliseconds = value % 1000;
                        struct tm *timeinfo = localtime(&seconds);
                        char timeStr[80];
                        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
                        printf("%s.%03d\n", timeStr, milliseconds);
                    } else {
                        printf("%lld\n", value);
                    }
                    break;
                }
                case RSCP::eTypeUInt64: {
                    unsigned long long value = (unsigned long long)protocol->getValueAsUInt64(response);
                    // Check if value looks like a millisecond timestamp (between 2020-2040)
                    if (!g_ctx.quietMode && value > 1577836800000ULL && value < 2209075200000ULL) {
                        time_t seconds = (time_t)(value / 1000);
                        int milliseconds = value % 1000;
                        struct tm *timeinfo = localtime(&seconds);
                        char timeStr[80];
                        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
                        printf("%s.%03d\n", timeStr, milliseconds);
                    } else {
                        printf("%llu\n", value);
                    }
                    break;
                }
                case RSCP::eTypeFloat32: {
                    float value = protocol->getValueAsFloat32(response);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%.2f", value);
                    printFormattedValue(response->tag, buf, (int64_t)value);
                    break;
                }
                case RSCP::eTypeDouble64:
                    printf("%.2f\n", protocol->getValueAsDouble64(response));
                    break;
                case RSCP::eTypeBitfield:
                    printf("0x%08X\n", protocol->getValueAsUInt32(response));
                    break;
                case RSCP::eTypeString: {
                    std::string str = protocol->getValueAsString(response);
                    printf("%s\n", str.c_str());
                    break;
                }
                case RSCP::eTypeContainer: {
                    std::vector<SRscpValue> container = protocol->getValueAsContainer(response);
                    if (!g_ctx.quietMode) {
                        printf("Container (%zu Elemente)\n", container.size());
                        for(size_t i = 0; i < container.size(); ++i) {
                            printf("  [%zu] ", i);
                            handleResponseValue(protocol, &container[i]);
                        }
                    } else {
                        for(size_t i = 0; i < container.size(); ++i) {
                            handleResponseValue(protocol, &container[i]);
                        }
                    }
                    protocol->destroyValueData(container);
                    break;
                }
                case RSCP::eTypeByteArray: {
                    if (!g_ctx.quietMode) {
                        printf("ByteArray (Laenge: %d bytes): ", response->length);
                    }
                    int displayLength = response->length > 16 ? 16 : response->length;
                    for(int i = 0; i < displayLength; i++) {
                        printf("%02X ", response->data[i]);
                    }
                    if(response->length > 16) {
                        printf("... ");
                    }
                    printf("\n");
                    break;
                }
                case RSCP::eTypeTimestamp: {
                    SRscpTimestamp ts = protocol->getValueAsTimestamp(response);
                    if (!g_ctx.quietMode) {
                        time_t seconds = (time_t)ts.seconds;
                        struct tm *timeinfo = localtime(&seconds);
                        char timeStr[80];
                        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
                        printf("%s.%03u\n", timeStr, ts.nanoseconds / 1000000);
                    } else {
                        printf("%llu.%09u\n", (unsigned long long)ts.seconds, ts.nanoseconds);
                    }
                    break;
                }
                default:
                    if (!g_ctx.quietMode) {
                        printf("Unbekannter Datentyp %d\n", response->dataType);
                    }
                    break;
            }
        } else {
            printf("Unknown tag %08X\n", response->tag);
        }
        break;
    }
    return 0;
}

static int processReceiveBuffer(const unsigned char * ucBuffer, int iLength)
{
    RscpProtocol protocol;
    SRscpFrame frame;

    int iResult = protocol.parseFrame(ucBuffer, iLength, &frame);
    if(iResult < 0) {
        // check if frame length error occured
        // in that case the full frame length was not received yet
        // and the receive function must get more data
        if(iResult == RSCP::ERR_INVALID_FRAME_LENGTH) {
            return 0;
        }
        // otherwise a not recoverable error occured and the connection can be closed
        else {
            return iResult;
        }
    }

    int iProcessedBytes = iResult;

    // process each SRscpValue struct seperately
    for(size_t i = 0; i < frame.data.size(); i++) {
        handleResponseValue(&protocol, &frame.data[i]);
    }

    // destroy frame data and free memory
    protocol.destroyFrameData(frame);

    // returned processed amount of bytes
    return iProcessedBytes;
}

static void receiveLoop(bool & bStopExecution)
{
    //--------------------------------------------------------------------------------------------------------------
    // RSCP Receive Frame Block Data
    //--------------------------------------------------------------------------------------------------------------
    // setup a static dynamic buffer which is dynamically expanded (re-allocated) on demand
    // the data inside this buffer is not released when this function is left
    static int iReceivedBytes = 0;
    static std::vector<uint8_t> vecDynamicBuffer;

    // check how many RSCP frames are received, must be at least 1
    // multiple frames can only occur in this example if one or more frames are received with a big time delay
    // this should usually not occur but handling this is shown in this example
    int iReceivedRscpFrames = 0;
    while(!bStopExecution && ((iReceivedBytes > 0) || iReceivedRscpFrames == 0))
    {
        // check and expand buffer
        if((vecDynamicBuffer.size() - iReceivedBytes) < 4096) {
            // check maximum size
            if(vecDynamicBuffer.size() > RSCP_MAX_FRAME_LENGTH) {
                // something went wrong and the size is more than possible by the RSCP protocol
                printf("Maximum buffer size exceeded %lu\n", vecDynamicBuffer.size());
                bStopExecution = true;
                break;
            }
            // increase buffer size by 4096 bytes each time the remaining size is smaller than 4096
            vecDynamicBuffer.resize(vecDynamicBuffer.size() + 4096);
        }
        // receive data
        long iResult = SocketRecvData(iSocket, &vecDynamicBuffer[0] + iReceivedBytes, vecDynamicBuffer.size() - iReceivedBytes);
        if(iResult < 0)
        {
            // check errno for the error code to detect if this is a timeout or a socket error
            if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                // receive timed out -> continue with re-sending the initial block
                printf("Response receive timeout (retry)\n");
                break;
            }
            // socket error -> check errno for failure code if needed
            printf("Socket receive error. errno %i\n", errno);
            bStopExecution = true;
            break;
        }
        else if(iResult == 0)
        {
            // connection was closed regularly by peer
            // if this happens on startup each time the possible reason is
            // wrong AES password or wrong network subnet (adapt hosts.allow file required)
            printf("Connection closed by peer\n");
            bStopExecution = true;
            break;
        }
        // increment amount of received bytes
        iReceivedBytes += iResult;

        // process all received frames
        while (!bStopExecution)
        {
            // round down to a multiple of AES_BLOCK_SIZE
            int iLength = ROUNDDOWN(iReceivedBytes, AES_BLOCK_SIZE);
            // if not even 32 bytes were received then the frame is still incomplete
            if(iLength == 0) {
                break;
            }
            // resize temporary decryption buffer
            std::vector<uint8_t> decryptionBuffer;
            decryptionBuffer.resize(iLength);
            // initialize encryption sequence IV value with value of previous block
            aesDecrypter.SetIV(ucDecryptionIV, AES_BLOCK_SIZE);
            // decrypt data from vecDynamicBuffer to temporary decryptionBuffer
            aesDecrypter.Decrypt(&vecDynamicBuffer[0], &decryptionBuffer[0], iLength / AES_BLOCK_SIZE);

            // data was received, check if we received all data
            int iProcessedBytes = processReceiveBuffer(&decryptionBuffer[0], iLength);
            if(iProcessedBytes < 0) {
                // an error occured;
                printf("Error parsing RSCP frame: %i\n", iProcessedBytes);
                // stop execution as the data received is not RSCP data
                bStopExecution = true;
                break;

            }
            else if(iProcessedBytes > 0) {
                // round up the processed bytes as iProcessedBytes does not include the zero padding bytes
                iProcessedBytes = ROUNDUP(iProcessedBytes, AES_BLOCK_SIZE);
                // store the IV value from encrypted buffer for next block decryption
                memcpy(ucDecryptionIV, &vecDynamicBuffer[0] + iProcessedBytes - AES_BLOCK_SIZE, AES_BLOCK_SIZE);
                // move the encrypted data behind the current frame data (if any received) to the front
                memcpy(&vecDynamicBuffer[0], &vecDynamicBuffer[0] + iProcessedBytes, vecDynamicBuffer.size() - iProcessedBytes);
                // decrement the total received bytes by the amount of processed bytes
                iReceivedBytes -= iProcessedBytes;
                // increment a counter that a valid frame was received and
                // continue parsing process in case a 2nd valid frame is in the buffer as well
                iReceivedRscpFrames++;
            }
            else {
                // iProcessedBytes is 0
                // not enough data of the next frame received, go back to receive mode if iReceivedRscpFrames == 0
                // or transmit mode if iReceivedRscpFrames > 0
                break;
            }
        }
    }
}

static void mainLoop(void)
{
    RscpProtocol protocol;
    bool bStopExecution = false;
    int counter = 0;

    while(!bStopExecution)
    {
        //--------------------------------------------------------------------------------------------------------------
        // RSCP Transmit Frame Block Data
        //--------------------------------------------------------------------------------------------------------------
        SRscpFrameBuffer frameBuffer;
        memset(&frameBuffer, 0, sizeof(frameBuffer));

        // create an RSCP frame with requests to some example data
        createRequestExample(&frameBuffer);

        // check that frame data was created
        if(frameBuffer.dataLength > 0)
        {
            // resize temporary encryption buffer to a multiple of AES_BLOCK_SIZE
            std::vector<uint8_t> encryptionBuffer;
            encryptionBuffer.resize(ROUNDUP(frameBuffer.dataLength, AES_BLOCK_SIZE));
            // zero padding for data above the desired length
            memset(&encryptionBuffer[0] + frameBuffer.dataLength, 0, encryptionBuffer.size() - frameBuffer.dataLength);
            // copy desired data length
            memcpy(&encryptionBuffer[0], frameBuffer.data, frameBuffer.dataLength);
            // set continues encryption IV
            aesEncrypter.SetIV(ucEncryptionIV, AES_BLOCK_SIZE);
            // start encryption from encryptionBuffer to encryptionBuffer, blocks = encryptionBuffer.size() / AES_BLOCK_SIZE
            aesEncrypter.Encrypt(&encryptionBuffer[0], &encryptionBuffer[0], encryptionBuffer.size() / AES_BLOCK_SIZE);
            // save new IV for next encryption block
            memcpy(ucEncryptionIV, &encryptionBuffer[0] + encryptionBuffer.size() - AES_BLOCK_SIZE, AES_BLOCK_SIZE);

            // send data on socket
            int iResult = SocketSendData(iSocket, &encryptionBuffer[0], encryptionBuffer.size());
            if(iResult < 0) {
                printf("Socket send error %i. errno %i\n", iResult, errno);
                bStopExecution = true;
            }
            else {
                // go into receive loop and wait for response
                receiveLoop(bStopExecution);
                
                // After first receive, check if we need more DCB requests
                if (counter > 0) {
                    if (!g_ctx.needMoreDCBRequests) {
                        // No more requests needed - stop
                        bStopExecution = true;
                    }
                }
            }
        }
        // free frame buffer memory
        protocol.destroyFrameData(&frameBuffer);

        // main loop sleep / cycle time before next request (only if continuing)
        if (!bStopExecution) {
            sleep(1);
        }

        counter++;

    }
}

uint32_t getTagByName(const char* name) {
    // Zuerst in geladenen Tags suchen
    if (!loadedTags.empty()) {
        for (auto& catPair : loadedTags) {
            for (const auto& tag : catPair.second) {
                if (strcasecmp(name, tag.name.c_str()) == 0) {
                    return tag.hex;
                }
            }
        }
    }
    
    // Tag nicht gefunden
    fprintf(stderr, "FEHLER: Tag '%s' nicht in der Tags-Datei gefunden!\n", name);
    fprintf(stderr, "Bitte verwenden Sie './e3dcset -l 0' um verfügbare Tags anzuzeigen,\n");
    fprintf(stderr, "oder nutzen Sie direkt den Hex-Wert (z.B. -r 0x01000001).\n\n");
    exit(EXIT_FAILURE);
}

bool isRequestTag(uint32_t tag) {
    // REQUEST Tags haben im zweiten Byte (Bits 16-23) einen Wert < 0x80
    // RESPONSE Tags haben im zweiten Byte einen Wert >= 0x80
    uint8_t secondByte = (tag >> 16) & 0xFF;
    return secondByte < 0x80;
}

void printTagList(int category) {
    // Validierung mit Enum statt Magic Numbers
    if (category < CATEGORY_OVERVIEW || category >= CATEGORY_MAX) {
        fprintf(stderr, "\nFehler: Ungültige Kategorie %d\n\n", category);
        fprintf(stderr, "Verfügbare Kategorien:\n");
        fprintf(stderr, "  %d - Übersicht aller Kategorien\n", CATEGORY_OVERVIEW);
        for (int i = 0; i < NUM_CATEGORIES; i++) {
            fprintf(stderr, "  %d - %s (%s)\n", 
                    categoryDescriptors[i].id, 
                    categoryDescriptors[i].shortName, 
                    categoryDescriptors[i].fullName);
        }
        fprintf(stderr, "\nBeispiel: ./e3dcset -l 0  (Übersicht)\n");
        fprintf(stderr, "         ./e3dcset -l 1  (EMS Tags anzeigen)\n\n");
        exit(EXIT_FAILURE);
    }
    
    if (category == CATEGORY_OVERVIEW) {
        printf("\n=== Verfügbare RSCP Tag Kategorien ===\n\n");
        for (int i = 0; i < NUM_CATEGORIES; i++) {
            const CategoryDescriptor& desc = categoryDescriptors[i];
            printf("  %d - %s (%s)", desc.id, desc.shortName, desc.fullName);
            // Zeige Anzahl geladener Tags an, falls verfügbar
            if (!loadedTags.empty() && loadedTags.find(desc.id) != loadedTags.end()) {
                printf(" (%zu Tags geladen)", loadedTags[desc.id].size());
            }
            printf("\n");
        }
        printf("\n=== Verwendung ===\n");
        printf("  ./e3dcset -l <kategorie>     # Tag-Liste anzeigen\n");
        printf("  ./e3dcset -r <tag-name>      # Tag-Wert abfragen\n\n");
        printf("=== Beispiele ===\n");
        printf("  ./e3dcset -l %d               # EMS Tags anzeigen\n", CATEGORY_EMS);
        printf("  ./e3dcset -l %d               # Battery Tags anzeigen\n", CATEGORY_BAT);
        printf("  ./e3dcset -r EMS_POWER_PV    # PV-Leistung abfragen\n");
        printf("  ./e3dcset -r BAT_DATA        # Batterie-Daten abfragen\n");
        printf("  ./e3dcset -r EMS_BAT_SOC -q  # Nur Wert ausgeben\n\n");
        return;
    }
    
    // Find descriptor for this category
    const char* categoryName = "Unknown";
    for (int i = 0; i < NUM_CATEGORIES; i++) {
        if (categoryDescriptors[i].id == category) {
            categoryName = categoryDescriptors[i].fullName;
            break;
        }
    }
    
    printf("\n=== Kategorie %d: %s ===\n\n", category, categoryName);
    
    printf("%-30s %-12s %s\n", "Tag-Name", "Hex-Wert", "Beschreibung");
    printf("%-30s %-12s %s\n", "------------------------------", "------------", "---------------------------------------------");
    
    // Tags aus geladener Datei verwenden
    if (!loadedTags.empty() && loadedTags.find(category) != loadedTags.end()) {
        for (const auto& tag : loadedTags[category]) {
            printf("%-30s 0x%08X   %s\n", tag.name.c_str(), tag.hex, tag.description.c_str());
        }
    } else {
        fprintf(stderr, "\nFEHLER: Keine Tags für Kategorie %d gefunden!\n", category);
        fprintf(stderr, "Bitte überprüfen Sie die Tag-Datei.\n\n");
        exit(EXIT_FAILURE);
    }
    
    printf("\n=== Beispiele ===\n");
    if (category == CATEGORY_EMS) {
        printf("  ./e3dcset -r EMS_POWER_PV       # PV-Leistung abfragen\n");
        printf("  ./e3dcset -r EMS_BAT_SOC -q     # Batterie-SOC (nur Wert)\n");
    } else if (category == CATEGORY_BAT) {
        printf("  ./e3dcset -r BAT_DATA           # Batterie-Daten Container\n");
        printf("  ./e3dcset -r BAT_RSOC -q        # RSOC (nur Wert)\n");
    } else {
        printf("  ./e3dcset -r <tag-name>         # Tag-Wert abfragen\n");
        printf("  ./e3dcset -r <tag-name> -q      # Nur Wert ausgeben\n");
    }
    printf("  ./e3dcset -r 0x%08X       # Mit Hex-Wert\n\n", category == CATEGORY_EMS ? TAG_EMS_REQ_BAT_SOC : TAG_BAT_REQ_DATA);
}

void usage(void){
    fprintf(stderr, "\n   Usage: e3dcset [-c LadeLeistung] [-d EntladeLeistung] [-e LadungsMenge] [-a] [-p Pfad zur Konfigurationsdatei] [-t Pfad zur Tags-Datei]\n");
    fprintf(stderr, "          e3dcset -r TAG_NAME [-i Modul-Index] [-q] [-p Pfad zur Konfigurationsdatei] [-t Pfad zur Tags-Datei]\n");
    fprintf(stderr, "          e3dcset -m <Modul-Index> [-p Pfad zur Konfigurationsdatei]\n");
    fprintf(stderr, "          e3dcset -l [kategorie]\n");
    fprintf(stderr, "          e3dcset -H <typ> [-D datum] [-p Pfad zur Konfigurationsdatei]\n\n");
    fprintf(stderr, "   Optionen:\n");
    fprintf(stderr, "     -c  LadeLeistung in Watt setzen\n");
    fprintf(stderr, "     -d  EntladeLeistung in Watt setzen\n");
    fprintf(stderr, "     -e  Manuelle Ladungsmenge in Wh setzen (0 = stoppen)\n");
    fprintf(stderr, "     -a  Automatik-Modus aktivieren\n");
    fprintf(stderr, "     -r  Wert abfragen (Tag-Name, Named Tag oder Hex-Wert)\n");
    fprintf(stderr, "     -i  Batterie-Modul Index (0 = erstes Modul, Standard: 0)\n");
    fprintf(stderr, "     -m  Alle Werte eines Batterie-Moduls anzeigen (Modul-Info-Dump)\n");
    fprintf(stderr, "     -q  Quiet Mode - nur Wert ausgeben (für Scripting)\n");
    fprintf(stderr, "     -l  RSCP Tag-Liste anzeigen (ohne Argument: Übersicht, 1-8 = Kategorie)\n");
    fprintf(stderr, "     -p  Pfad zur Konfigurationsdatei (Standard: e3dcset.config)\n");
    fprintf(stderr, "     -t  Pfad zur Tags-Datei (Standard: e3dcset.tags)\n");
    fprintf(stderr, "     -H  Historische Daten abfragen (day/week/month/year)\n");
    fprintf(stderr, "     -D  Datum (Format: YYYY-MM-DD oder 'today', Standard: heute)\n\n");
    fprintf(stderr, "   Hinweis: -r, -m und -H können nicht mit -c, -d, -e oder -a kombiniert werden\n\n");
    fprintf(stderr, "   Beispiele:\n");
    fprintf(stderr, "     e3dcset -l                      # Kategorie-Übersicht\n");
    fprintf(stderr, "     e3dcset -l 1                    # EMS Tags anzeigen\n");
    fprintf(stderr, "     e3dcset -r EMS_POWER_PV         # PV-Leistung abfragen\n");
    fprintf(stderr, "     e3dcset -r EMS_BAT_SOC -q       # Batterie-SOC (nur Wert)\n");
    fprintf(stderr, "     e3dcset -r BAT_REQ_RSOC         # Batterie-SOC Modul 0\n");
    fprintf(stderr, "     e3dcset -r BAT_REQ_RSOC -i 1    # Batterie-SOC Modul 1\n");
    fprintf(stderr, "     e3dcset -r BAT_REQ_ASOC -i 0 -q # SOH Modul 0 (quiet)\n");
    fprintf(stderr, "     e3dcset -m 0                    # Alle Werte von Modul 0\n");
    fprintf(stderr, "     e3dcset -m 1                    # Alle Werte von Modul 1\n");
    fprintf(stderr, "     e3dcset -r 0x01000008           # Mit Hex-Wert\n");
    fprintf(stderr, "     e3dcset -H day                  # Heutige Tagesdaten\n");
    fprintf(stderr, "     e3dcset -H day -D 2024-11-20    # Tagesdaten vom 20.11.2024\n");
    fprintf(stderr, "     e3dcset -t /path/custom.tags -l 1  # Custom Tags-Datei verwenden\n\n");
    exit(EXIT_FAILURE);
}

void readConfig(void){

    FILE *fp;

    fp = fopen(g_ctx.configPath, "r");

    char var[128], value[128], line[256];

    if(fp) {

        while (fgets(line, sizeof(line), fp)) {

                memset(var, 0, sizeof(var));
                memset(value, 0, sizeof(value));

                if(sscanf(line, "%[^ \t=]%*[\t ]=%*[\t ]%[^\n]", var, value) == 2) {

                        if(strcmp(var, "MIN_LEISTUNG") == 0)
                                e3dc_config.MIN_LEISTUNG = atoi(value);
 
                        else if(strcmp(var, "MAX_LEISTUNG") == 0)
                                e3dc_config.MAX_LEISTUNG = atoi(value);

                        else if(strcmp(var, "MIN_LADUNGSMENGE") == 0)
                                e3dc_config.MIN_LADUNGSMENGE = atoi(value);

                        else if(strcmp(var, "MAX_LADUNGSMENGE") == 0)
                                e3dc_config.MAX_LADUNGSMENGE = atoi(value);

                        else if(strcmp(var, "server_ip") == 0)
                                strcpy(e3dc_config.server_ip, value);

                        else if(strcmp(var, "server_port") == 0)
                                e3dc_config.server_port = atoi(value);

                        else if(strcmp(var, "e3dc_user") == 0)
                                strcpy(e3dc_config.e3dc_user, value);

                        else if(strcmp(var, "e3dc_password") == 0)
                                strcpy(e3dc_config.e3dc_password, value);

                        else if(strcmp(var, "aes_password") == 0)
                                strcpy(e3dc_config.aes_password, value);

                        else if(strcmp(var, "debug") == 0)
                                debug = atoi(value);
                }
            }

        DEBUG(" \n");
        DEBUG("----------------------------------------------------------\n");
        DEBUG("Gelesene Parameter aus Konfigurationsdatei %s:\n", g_ctx.configPath);
        DEBUG("MIN_LEISTUNG=%u\n",e3dc_config.MIN_LEISTUNG);
        DEBUG("MAX_LEISTUNG=%u\n",e3dc_config.MAX_LEISTUNG);
        DEBUG("MIN_LADUNGSMENGE=%u\n",e3dc_config.MIN_LADUNGSMENGE);
        DEBUG("MAX_LADUNGSMENGE=%u\n",e3dc_config.MAX_LADUNGSMENGE);
        DEBUG("server_ip=%s\n",e3dc_config.server_ip);
        DEBUG("server_port=%i\n",e3dc_config.server_port);
        DEBUG("e3dc_user=%s\n", strlen(e3dc_config.e3dc_user) > 0 ? "***@***" : "");
        DEBUG("e3dc_password=%s\n", strlen(e3dc_config.e3dc_password) > 0 ? "********" : "");
        DEBUG("aes_password=%s\n", strlen(e3dc_config.aes_password) > 0 ? "********" : "");
        DEBUG("----------------------------------------------------------\n");

        fclose(fp);

    } else {

        printf("Konfigurationsdatei %s wurde nicht gefunden.\n\n",g_ctx.configPath);
        exit(EXIT_FAILURE);
    }

}

void checkArguments(void){

    if (g_ctx.werteAbfragen && (g_ctx.leistungAendern || g_ctx.manuelleSpeicherladung)){
        fprintf(stderr, "[-r] kann nicht zusammen mit [-c], [-d], [-e] oder [-a] verwendet werden\n\n");
        exit(EXIT_FAILURE);
    }
    
    if (g_ctx.historieAbfrage && (g_ctx.leistungAendern || g_ctx.manuelleSpeicherladung || g_ctx.werteAbfragen)){
        fprintf(stderr, "[-H] kann nicht zusammen mit [-r], [-c], [-d], [-e] oder [-a] verwendet werden\n\n");
        exit(EXIT_FAILURE);
    }

    if (g_ctx.werteAbfragen && g_ctx.leseTag == 0){
        fprintf(stderr, "[-r] benoetigt einen gueltigen TAG-Wert (z.B. 0x01000001 oder battery-soc)\n\n");
        exit(EXIT_FAILURE);
    }

    if (g_ctx.quietMode && !g_ctx.werteAbfragen){
        fprintf(stderr, "[-q] kann nur zusammen mit [-r] verwendet werden\n\n");
        exit(EXIT_FAILURE);
    }
    
    if (g_ctx.historieDatum && !g_ctx.historieAbfrage){
        fprintf(stderr, "[-D] kann nur zusammen mit [-H] verwendet werden\n\n");
        exit(EXIT_FAILURE);
    }
    
    if (g_ctx.historieAbfrage && !g_ctx.historieTyp){
        fprintf(stderr, "[-H] benötigt einen History-Typ (day, week, month, year)\n\n");
        exit(EXIT_FAILURE);
    }
    
    if (g_ctx.historieAbfrage && !g_ctx.historieDatum){
        g_ctx.historieDatum = strdup("today");
    }

    if (g_ctx.ladeLeistungGesetzt && (g_ctx.ladeLeistung < 0 || g_ctx.ladeLeistung < e3dc_config.MIN_LEISTUNG || g_ctx.ladeLeistung > e3dc_config.MAX_LEISTUNG)){
        fprintf(stderr, "[-c g_ctx.ladeLeistung] muss zwischen %i und %i liegen\n\n", e3dc_config.MIN_LEISTUNG, e3dc_config.MAX_LEISTUNG);
        exit(EXIT_FAILURE);
    }

    if (g_ctx.entladeLeistungGesetzt && (g_ctx.entladeLeistung < 0 || g_ctx.entladeLeistung < e3dc_config.MIN_LEISTUNG || g_ctx.entladeLeistung > e3dc_config.MAX_LEISTUNG)){
        fprintf(stderr, "[-d g_ctx.entladeLeistung] muss zwischen %i und %i liegen\n\n", e3dc_config.MIN_LEISTUNG, e3dc_config.MAX_LEISTUNG);
        exit(EXIT_FAILURE);
    }

    if (g_ctx.automatischLeistungEinstellen && (g_ctx.entladeLeistung > 0 || g_ctx.ladeLeistung > 0)){
        fprintf(stderr, "bei Lade/Entladeleistung Automatik [-a] duerfen [-c g_ctx.ladeLeistung] und [-d g_ctx.entladeLeistung] nicht gesetzt sein\n\n");
        exit(EXIT_FAILURE);
    }

    if (g_ctx.manuelleSpeicherladung && (g_ctx.ladungsMenge < e3dc_config.MIN_LADUNGSMENGE || g_ctx.ladungsMenge > e3dc_config.MAX_LADUNGSMENGE)){
        fprintf(stderr, "Fuer die manuelle Speicherladung muss der angegebene Wert zwischen %iWh und %iWh liegen\n\n",e3dc_config.MIN_LADUNGSMENGE,e3dc_config.MAX_LADUNGSMENGE);
        exit(EXIT_FAILURE);
    }

    if (!g_ctx.leistungAendern && !g_ctx.manuelleSpeicherladung && !g_ctx.werteAbfragen && !g_ctx.historieAbfrage && !g_ctx.modulInfoDump){
        fprintf(stderr, "Keine Verbindung mit Server erforderlich\n\n");
        exit(EXIT_FAILURE);
    }

}

void connectToServer(void){

    DEBUG("Connecting to server %s:%i\n", e3dc_config.server_ip, e3dc_config.server_port);

    iSocket = SocketConnect(e3dc_config.server_ip, e3dc_config.server_port);

    if(iSocket < 0) {
        printf("Connection failed\n");
        exit(EXIT_FAILURE);
    }
    DEBUG("Connected successfully\n");

    // create AES key and set AES parameters
    {
        // initialize AES encryptor and decryptor IV
        memset(ucDecryptionIV, 0xff, AES_BLOCK_SIZE);
        memset(ucEncryptionIV, 0xff, AES_BLOCK_SIZE);

        // limit password length to AES_KEY_SIZE
        int64_t iPasswordLength = strlen(e3dc_config.aes_password);
        if(iPasswordLength > AES_KEY_SIZE)
                iPasswordLength = AES_KEY_SIZE;

        // copy up to 32 bytes of AES key password
        uint8_t ucAesKey[AES_KEY_SIZE];
        memset(ucAesKey, 0xff, AES_KEY_SIZE);
        memcpy(ucAesKey, e3dc_config.aes_password, iPasswordLength);

        // set encryptor and decryptor parameters
        aesDecrypter.SetParameters(AES_KEY_SIZE * 8, AES_BLOCK_SIZE * 8);
        aesEncrypter.SetParameters(AES_KEY_SIZE * 8, AES_BLOCK_SIZE * 8);
        aesDecrypter.StartDecryption(ucAesKey);
        aesEncrypter.StartEncryption(ucAesKey);
    }

}

int main(int argc, char *argv[])
{

        // Argumente der Kommandozeile parsen
    
    if (argc == 1){
        usage();
        }
    
    int opt;

    while ((opt = getopt(argc, argv, "c:d:e:ap:r:i:m:qlt:H:D:I:S:")) != -1) {

        switch (opt) {

        case 'c':
                g_ctx.leistungAendern = true;
                g_ctx.ladeLeistungGesetzt = true;
                g_ctx.ladeLeistung = atoi(optarg);
                break;
        case 'd':
                g_ctx.leistungAendern = true;
            g_ctx.entladeLeistungGesetzt = true;
                g_ctx.entladeLeistung = atoi(optarg);
                break;
        case 'e':
                g_ctx.manuelleSpeicherladung = true;
                g_ctx.ladungsMenge = atoi(optarg);
                break;
        case 'a':
                g_ctx.leistungAendern = true;
                g_ctx.automatischLeistungEinstellen = true;
                break;
        case 'p':
                g_ctx.configPath = strdup(optarg);
                break;
        case 't':
                g_ctx.tagfilePath = strdup(optarg);
                break;
        case 'H':
                g_ctx.historieAbfrage = true;
                g_ctx.historieTyp = strdup(optarg);
                if (strcmp(optarg, "day") != 0 && strcmp(optarg, "week") != 0 &&
                    strcmp(optarg, "month") != 0 && strcmp(optarg, "year") != 0) {
                    fprintf(stderr, "Fehler: Ungültiger History-Typ '%s'\n", optarg);
                    fprintf(stderr, "Gültige Typen: day, week, month, year\n");
                    exit(EXIT_FAILURE);
                }
                break;
        case 'D':
                g_ctx.historieDatum = strdup(optarg);
                break;
        case 'r':
                g_ctx.werteAbfragen = true;
                if (optarg[0] >= '0' && optarg[0] <= '9') {
                    // Hex-Wert direkt parsen
                    g_ctx.leseTag = strtoul(optarg, NULL, 0);
                    // Validierung: Nur REQUEST Tags können abgefragt werden
                    if (!isRequestTag(g_ctx.leseTag)) {
                        fprintf(stderr, "Fehler: 0x%08X ist ein RESPONSE Tag!\n", g_ctx.leseTag);
                        fprintf(stderr, "Sie können nur REQUEST Tags abfragen (zweites Byte < 0x80).\n");
                        fprintf(stderr, "Beispiel: 0x01000008 (REQUEST), nicht 0x01800008 (RESPONSE)\n");
                        exit(EXIT_FAILURE);
                    }
                } else {
                    // Tag-Namen speichern für spätere Konvertierung (nach loadTagsFile)
                    g_ctx.tagName = strdup(optarg);
                }
                break;
        case 'i':
                g_ctx.batIndex = (uint16_t)atoi(optarg);
                break;
        case 'm':
                g_ctx.modulInfoDump = true;
                g_ctx.batIndex = (uint16_t)atoi(optarg);
                break;
        case 'q':
                g_ctx.quietMode = true;
                break;
        case 'l':
                g_ctx.listTags = true;
                // Prüfe nächstes Argument als optional Kategorie
                if (optind < argc && argv[optind][0] >= '0' && argv[optind][0] <= '9') {
                    g_ctx.listCategory = atoi(argv[optind]);
                    optind++;  // Skip this argument
                    if (g_ctx.listCategory < 1 || g_ctx.listCategory > 8) {
                        fprintf(stderr, "Fehler: Ungültige Kategorie %d (gültig: 1-8)\n", g_ctx.listCategory);
                        fprintf(stderr, "Beispiel: ./e3dcset -l    (Übersicht)\n");
                        fprintf(stderr, "         ./e3dcset -l 1  (EMS Tags)\n\n");
                        usage();
                    }
                } else {
                    // Keine Kategorie angegeben → Übersicht
                    g_ctx.listCategory = 0;
                }
                break;
                default:
                usage();

        }
    }

    if (optind < argc){
        usage();
    }

    // Lade Tag-Definitionen aus Datei VOR dem -l Check
    loadTagsFile(g_ctx.tagfilePath);

    // Handle -l option early (no device connection needed)
    if (g_ctx.listTags) {
        printTagList(g_ctx.listCategory);
        return 0;
    }
    
    // Konvertiere Tag-Namen zu Hex-Wert (nach loadTagsFile)
    if (g_ctx.werteAbfragen && g_ctx.tagName != NULL) {
        g_ctx.leseTag = getTagByName(g_ctx.tagName);
        // Validierung: Nur REQUEST Tags können abgefragt werden
        if (!isRequestTag(g_ctx.leseTag)) {
            fprintf(stderr, "Fehler: 0x%08X ist ein RESPONSE Tag!\n", g_ctx.leseTag);
            fprintf(stderr, "Sie können nur REQUEST Tags abfragen (zweites Byte < 0x80).\n");
            fprintf(stderr, "Beispiel: 0x01000008 (REQUEST), nicht 0x01800008 (RESPONSE)\n");
            exit(EXIT_FAILURE);
        }
        free(g_ctx.tagName);
        g_ctx.tagName = NULL;
    }

    // Lese Konfigurationsdatei
    readConfig();

    // Argumente der Kommandozeile plausibilisieren
    checkArguments();

    // Verbinde mit Hauskraftwerk
    connectToServer();

    // Starte Sende- / Empfangsschleife
    mainLoop();

    // Trenne Verbindung zum Hauskraftwerk
    SocketClose(iSocket);
    
    DEBUG("Ende!\n\n");

    return 0;
}

