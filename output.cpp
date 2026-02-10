#include "output.h"
#include "config.h"
#include "rscp_handler.h"
#include "history.h"
#include "RscpTags.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

// Global tag storage
std::map<int, std::vector<TagInfo>> loadedTags;
std::map<std::string, std::string> loadedInterpretations;
BatteryModuleData g_batteryData;
bool g_jsonFirstField = true;

// Category descriptors
const CategoryDescriptor categoryDescriptors[] = {
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
const int NUM_CATEGORIES = sizeof(categoryDescriptors) / sizeof(categoryDescriptors[0]);

void jsonStart() {
    printf("{\n");
    g_jsonFirstField = true;
}

void jsonEnd() {
    printf("\n}\n");
    g_jsonFirstField = true;
}

void jsonField(const char* key, const char* value, bool isString) {
    if (!g_jsonFirstField) {
        printf(",\n");
    }
    g_jsonFirstField = false;
    
    if (isString) {
        // Escape quotes and backslashes in value
        printf("  \"%s\": \"", key);
        for (const char* p = value; *p; p++) {
            if (*p == '"' || *p == '\\') printf("\\");
            printf("%c", *p);
        }
        printf("\"");
    } else {
        printf("  \"%s\": %s", key, value);
    }
}

void jsonFieldInt(const char* key, int64_t value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lld", (long long)value);
    jsonField(key, buf, false);
}

void jsonFieldFloat(const char* key, double value) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.2f", value);
    jsonField(key, buf, false);
}

// RSCP Error Code Descriptions
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


