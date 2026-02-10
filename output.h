#ifndef OUTPUT_H
#define OUTPUT_H

#include <stdint.h>
#include <string>
#include <vector>
#include <map>
#include "RscpProtocol.h"

// Tag-Kategorien als Enum
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
    CATEGORY_MAX = 10
};

// Kategorie-Deskriptoren
struct CategoryDescriptor {
    int id;
    const char* shortName;
    const char* fullName;
};

extern const CategoryDescriptor categoryDescriptors[];
extern const int NUM_CATEGORIES;

// Tag-Informationen
struct TagInfo {
    std::string name;
    uint32_t hex;
    std::string description;
};

// DCB-Daten-Struktur für Multi-Request-Sammlung
struct DCBData {
    uint8_t index;
    std::vector<std::pair<uint32_t, SRscpValue>> tags;
};

// Batterie-Modul-Daten-Struktur
struct BatteryModuleData {
    std::map<uint32_t, SRscpValue> batteryTags;
    std::vector<DCBData> dcbs;
    uint8_t dcbCount;
};

// Global tag storage (defined in output.cpp)
extern std::map<int, std::vector<TagInfo>> loadedTags;
extern std::map<std::string, std::string> loadedInterpretations;
extern BatteryModuleData g_batteryData;
extern bool g_jsonFirstField;

// JSON Output Helpers
void jsonStart();
void jsonEnd();
void jsonField(const char* key, const char* value, bool isString = true);
void jsonFieldInt(const char* key, int64_t value);
void jsonFieldFloat(const char* key, double value);

// Tag handling
void loadTagsFile(const char* filename);
const char* getTagDescription(uint32_t tag);
const char* interpretValue(uint32_t tag, int64_t value);
void printFormattedValue(uint32_t tag, const char* valueStr, int64_t numericValue);
uint32_t getTagByName(const char* name);
bool isRequestTag(uint32_t tag);
void printTagList(int category);

#endif // OUTPUT_H
