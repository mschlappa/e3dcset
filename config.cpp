#include "config.h"
#include "rscp_handler.h"
#include "output.h"
#include "SocketConnection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <strings.h>

// Global configuration instances
e3dc_config_t e3dc_config;
bool debug = false;
CommandContext g_ctx;

// CommandContext constructor
CommandContext::CommandContext() : 
    leistungAendern(false),
    automatischLeistungEinstellen(false),
    ladeLeistungGesetzt(false),
    entladeLeistungGesetzt(false),
    manuelleSpeicherladung(false),
    werteAbfragen(false),
    quietMode(false),
    jsonOutput(false),
    listTags(false),
    listCategory(0),
    historieAbfrage(false),
    batContainerQuery(false),
    modulInfoDump(false),
    setEPReserve(false),
    needMoreDCBRequests(false),
    currentDCBIndex(0),
    totalDCBs(0),
    isFirstModuleDumpRequest(true),
    dcbRequestRetries(0),
    ladungsMenge(0),
    ladeLeistung(0),
    entladeLeistung(0),
    leseTag(0),
    batIndex(0),
    epReserveWh(0.0f),
    historieInterval(HISTORY_INTERVAL_DAY),
    historieSpan(HISTORY_SPAN_DAY),
    historieStartTime(0),
    configPath(safe_strdup("e3dcset.config", "CommandContext constructor")),
    tagfilePath(safe_strdup("e3dcset.tags", "CommandContext constructor")),
    tagName(NULL),
    historieDatum(NULL),
    historieTyp(NULL)
{}

// CommandContext destructor
CommandContext::~CommandContext() {
    free(configPath);
    free(tagfilePath);
    free(tagName);
    free(historieDatum);
    free(historieTyp);
}

void usage(void){
    fprintf(stderr, "\n");
    fprintf(stderr, "   e3dcset - Ein Tool zum Steuern und Abfragen des E3/DC Hauskraftwerks\n\n");
    fprintf(stderr, "   Verwendung:   e3dcset [Optionen]\n\n");
    fprintf(stderr, "   Optionen:\n");
    fprintf(stderr, "     -c  Lade-Leistung setzen (0 = keine Ladung, >0 = Watt)\n");
    fprintf(stderr, "     -d  Entlade-Leistung setzen (0 = keine Entladung, >0 = Watt)\n");
    fprintf(stderr, "     -a  Lade/Entlade-Leistung Automatik (setzt beide auf 0)\n");
    fprintf(stderr, "     -e  Manuelle Speicherladung (Wh)\n");
    fprintf(stderr, "     -E  Notstromreserve setzen (Wh, 0 = deaktiviert)\n");
    fprintf(stderr, "     -r  Bestimmten Tag abfragen (Hex-Wert oder Name, z.B. EMS_POWER_PV)\n");
    fprintf(stderr, "     -i  Batterie-Modul Index (0 = erstes Modul, Standard: 0)\n");
    fprintf(stderr, "     -m  Alle Werte eines Batterie-Moduls anzeigen (Modul-Info-Dump)\n");
    fprintf(stderr, "     -q  Quiet Mode - nur Wert ausgeben (für Scripting)\n");
    fprintf(stderr, "     -j  JSON Output - strukturierte Ausgabe für Scripting\n");
    fprintf(stderr, "     -l  RSCP Tag-Liste anzeigen (ohne Argument: Übersicht, 1-8 = Kategorie)\n");
    fprintf(stderr, "     -p  Pfad zur Konfigurationsdatei (Standard: e3dcset.config)\n");
    fprintf(stderr, "     -t  Pfad zur Tags-Datei (Standard: e3dcset.tags)\n");
    fprintf(stderr, "     -H  Historische Daten abfragen (day/week/month/year)\n");
    fprintf(stderr, "     -D  Datum (Format: YYYY-MM-DD oder 'today', Standard: heute)\n\n");
    fprintf(stderr, "   Hinweis: -r, -m und -H können nicht mit -c, -d, -e, -E oder -a kombiniert werden\n\n");
    fprintf(stderr, "   Beispiele:\n");
    fprintf(stderr, "     e3dcset -l                      # Kategorie-Übersicht\n");
    fprintf(stderr, "     e3dcset -l 1                    # EMS Tags anzeigen\n");
    fprintf(stderr, "     e3dcset -r EMS_POWER_PV         # PV-Leistung abfragen\n");
    fprintf(stderr, "     e3dcset -r EMS_BAT_SOC -q       # Batterie-SOC (nur Wert)\n");
    fprintf(stderr, "     e3dcset -r EMS_POWER_PV -j      # PV-Leistung als JSON\n");
    fprintf(stderr, "     e3dcset -r BAT_REQ_RSOC         # Batterie-SOC Modul 0\n");
    fprintf(stderr, "     e3dcset -r BAT_REQ_RSOC -i 1    # Batterie-SOC Modul 1\n");
    fprintf(stderr, "     e3dcset -r BAT_REQ_ASOC -i 0 -q # SOH Modul 0 (quiet)\n");
    fprintf(stderr, "     e3dcset -m 0                    # Alle Werte von Modul 0\n");
    fprintf(stderr, "     e3dcset -m 0 -j                 # Alle Werte als JSON\n");
    fprintf(stderr, "     e3dcset -m 1                    # Alle Werte von Modul 1\n");
    fprintf(stderr, "     e3dcset -r 0x01000008           # Mit Hex-Wert\n");
    fprintf(stderr, "     e3dcset -H day                  # Heutige Tagesdaten\n");
    fprintf(stderr, "     e3dcset -H day -j               # Tagesdaten als JSON\n");
    fprintf(stderr, "     e3dcset -H day -D 2024-11-20    # Tagesdaten vom 20.11.2024\n");
    fprintf(stderr, "     e3dcset -E 2600                 # Notstromreserve auf 2600 Wh setzen\n");
    fprintf(stderr, "     e3dcset -E 0                    # Notstromreserve deaktivieren\n");
    fprintf(stderr, "     e3dcset -t /path/custom.tags -l 1  # Custom Tags-Datei verwenden\n\n");
    exit(EXIT_FAILURE);
}

// Check config file permissions and warn if too permissive
void checkConfigPermissions(const char* config_path) {
    struct stat st;
    
    if (stat(config_path, &st) == 0) {
        mode_t perms = st.st_mode & 0777;
        
        // Warn if readable by group or others (not just owner)
        if (perms & (S_IRWXG | S_IRWXO)) {
            fprintf(stderr, "\n⚠️  SECURITY WARNING: Config file has overly permissive access rights!\n");
            fprintf(stderr, "    File: %s\n", config_path);
            fprintf(stderr, "    Current permissions: %03o\n", perms);
            fprintf(stderr, "    The config file contains plaintext passwords.\n");
            fprintf(stderr, "    Recommended: chmod 600 %s\n", config_path);
            fprintf(stderr, "    (Only owner should read/write)\n\n");
        }
    }
}

// Try to load value from environment variable, return NULL if not set
const char* getEnvOrNull(const char* var_name) {
    const char* value = getenv(var_name);
    if (value && strlen(value) > 0) {
        DEBUG("Using environment variable %s\n", var_name);
        return value;
    }
    return NULL;
}

// Sichere String-Kopie mit Längenvalidierung
bool safe_string_copy(char* dest, size_t dest_size, const char* src, const char* field_name) {
    size_t src_len = strlen(src);
    
    if (src_len >= dest_size) {
        fprintf(stderr, "FEHLER: Config-Wert für '%s' zu lang (max %zu Zeichen, gelesen: %zu)\n",
                field_name, dest_size - 1, src_len);
        fprintf(stderr, "        Bitte Config-Datei überprüfen.\n");
        return false;
    }
    
    strncpy(dest, src, dest_size - 1);
    dest[dest_size - 1] = '\0';
    return true;
}

// Sanitize sensitive string for safe logging
const char* sanitizeCredential(const char* credential) {
    return (credential && strlen(credential) > 0) ? "********" : "";
}

// Safe strdup with null-pointer check
char* safe_strdup(const char* str, const char* context) {
    if (str == NULL) {
        fprintf(stderr, "FEHLER: Versuch NULL-String zu duplizieren (%s)\n", 
                context ? context : "unbekannter Kontext");
        exit(EXIT_FAILURE);
    }
    
    char* result = strdup(str);
    if (result == NULL) {
        fprintf(stderr, "FEHLER: Speicher-Allokation fehlgeschlagen für '%s' (%s)\n",
                str, context ? context : "unbekannter Kontext");
        exit(EXIT_FAILURE);
    }
    
    return result;
}

void readConfig(void){
    FILE *fp;
    
    checkConfigPermissions(g_ctx.configPath);
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
                else if(strcmp(var, "server_ip") == 0) {
                    if (!safe_string_copy(e3dc_config.server_ip, sizeof(e3dc_config.server_ip), value, "server_ip"))
                        exit(EXIT_FAILURE);
                }
                else if(strcmp(var, "server_port") == 0)
                    e3dc_config.server_port = atoi(value);
                else if(strcmp(var, "e3dc_user") == 0) {
                    if (!safe_string_copy(e3dc_config.e3dc_user, sizeof(e3dc_config.e3dc_user), value, "e3dc_user"))
                        exit(EXIT_FAILURE);
                }
                else if(strcmp(var, "e3dc_password") == 0) {
                    if (!safe_string_copy(e3dc_config.e3dc_password, sizeof(e3dc_config.e3dc_password), value, "e3dc_password"))
                        exit(EXIT_FAILURE);
                }
                else if(strcmp(var, "aes_password") == 0) {
                    if (!safe_string_copy(e3dc_config.aes_password, sizeof(e3dc_config.aes_password), value, "aes_password"))
                        exit(EXIT_FAILURE);
                }
                else if(strcmp(var, "debug") == 0)
                    debug = atoi(value);
            }
        }

        // Environment variables can override config values
        const char* env_user = getEnvOrNull("E3DC_USER");
        const char* env_password = getEnvOrNull("E3DC_PASSWORD");
        const char* env_aes = getEnvOrNull("E3DC_AES_PASSWORD");
        
        if (env_user) {
            if (!safe_string_copy(e3dc_config.e3dc_user, sizeof(e3dc_config.e3dc_user), env_user, "E3DC_USER (env)"))
                exit(EXIT_FAILURE);
        }
        if (env_password) {
            if (!safe_string_copy(e3dc_config.e3dc_password, sizeof(e3dc_config.e3dc_password), env_password, "E3DC_PASSWORD (env)"))
                exit(EXIT_FAILURE);
        }
        if (env_aes) {
            if (!safe_string_copy(e3dc_config.aes_password, sizeof(e3dc_config.aes_password), env_aes, "E3DC_AES_PASSWORD (env)"))
                exit(EXIT_FAILURE);
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
        DEBUG("e3dc_password=%s\n", sanitizeCredential(e3dc_config.e3dc_password));
        DEBUG("aes_password=%s\n", sanitizeCredential(e3dc_config.aes_password));
        DEBUG("----------------------------------------------------------\n");

        fclose(fp);
    } else {
        printf("Konfigurationsdatei %s wurde nicht gefunden.\n\n",g_ctx.configPath);
        exit(EXIT_FAILURE);
    }
}

void checkArguments(void){
    if (g_ctx.werteAbfragen && (g_ctx.leistungAendern || g_ctx.manuelleSpeicherladung || g_ctx.setEPReserve)){
        fprintf(stderr, "[-r] kann nicht zusammen mit [-c], [-d], [-e], [-E] oder [-a] verwendet werden\n\n");
        exit(EXIT_FAILURE);
    }
    
    if (g_ctx.historieAbfrage && (g_ctx.leistungAendern || g_ctx.manuelleSpeicherladung || g_ctx.werteAbfragen || g_ctx.setEPReserve)){
        fprintf(stderr, "[-H] kann nicht zusammen mit [-r], [-c], [-d], [-e], [-E] oder [-a] verwendet werden\n\n");
        exit(EXIT_FAILURE);
    }
    
    if (g_ctx.setEPReserve && g_ctx.epReserveWh < 0){
        fprintf(stderr, "[-E] Notstromreserve muss >= 0 Wh sein\n\n");
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
        g_ctx.historieDatum = safe_strdup("today", "default history date");
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

    if (!g_ctx.leistungAendern && !g_ctx.manuelleSpeicherladung && !g_ctx.werteAbfragen && !g_ctx.historieAbfrage && !g_ctx.modulInfoDump && !g_ctx.setEPReserve){
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
