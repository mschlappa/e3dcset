#ifndef CONFIG_H
#define CONFIG_H

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// SECURITY WARNING: This struct contains sensitive credentials
// Never log or print e3dc_user, e3dc_password, or aes_password in plain text
// Always use sanitizeCredential() for any debug/log output
typedef struct {
    uint32_t MIN_LEISTUNG;
    uint32_t MAX_LEISTUNG;
    uint32_t MIN_LADUNGSMENGE;
    uint32_t MAX_LADUNGSMENGE;
    char         server_ip[20];
    uint32_t server_port;
    char         e3dc_user[128];         // SENSITIVE: Do not log!
    char         e3dc_password[128];     // SENSITIVE: Do not log!
    char         aes_password[128];      // SENSITIVE: Do not log!
    bool         debug;
} e3dc_config_t;

// History defaults
#define HISTORY_INTERVAL_DAY      900     // 15 minutes
#define HISTORY_SPAN_DAY          86400   // 24 hours
#define HISTORY_INTERVAL_WEEK     3600    // 1 hour
#define HISTORY_SPAN_WEEK         604800  // 7 days
#define HISTORY_INTERVAL_MONTH    86400   // 1 day
#define HISTORY_SPAN_MONTH        2678400 // 31 days
#define HISTORY_INTERVAL_YEAR     604800  // 1 week
#define HISTORY_SPAN_YEAR         31536000// 365 days

// Command Context - encapsulates all command line state
struct CommandContext {
    // Control modes
    bool leistungAendern;
    bool automatischLeistungEinstellen;
    bool ladeLeistungGesetzt;
    bool entladeLeistungGesetzt;
    bool manuelleSpeicherladung;
    bool werteAbfragen;
    bool quietMode;
    bool jsonOutput;
    bool listTags;
    int listCategory;
    bool historieAbfrage;
    bool batContainerQuery;
    bool modulInfoDump;
    bool setEPReserve;
    
    // Multi-DCB support
    bool needMoreDCBRequests;
    uint8_t currentDCBIndex;
    uint8_t totalDCBs;
    bool isFirstModuleDumpRequest;
    
    // Power and energy settings
    uint32_t ladungsMenge;
    uint32_t ladeLeistung;
    uint32_t entladeLeistung;
    uint32_t leseTag;
    uint16_t batIndex;
    float epReserveWh;
    
    // History query parameters
    char *historieDatum;
    char *historieTyp;
    uint32_t historieInterval;
    uint32_t historieSpan;
    time_t historieStartTime;
    
    // Configuration paths
    char *configPath;
    char *tagfilePath;
    char *tagName;
    
    // Constructor with defaults
    CommandContext();
    
    // Destructor - free all dynamically allocated strings
    ~CommandContext();
};

// Global configuration (defined in config.cpp)
extern e3dc_config_t e3dc_config;
extern bool debug;
extern CommandContext g_ctx;

// Configuration functions
void checkConfigPermissions(const char* config_path);
const char* getEnvOrNull(const char* var_name);
bool safe_string_copy(char* dest, size_t dest_size, const char* src, const char* field_name);
const char* sanitizeCredential(const char* credential);
char* safe_strdup(const char* str, const char* context);
void readConfig(void);
void checkArguments(void);
void connectToServer(void);
void usage(void);

#endif // CONFIG_H
