#include "rscp_handler.h"
#include "config.h"
#include "output.h"
#include "history.h"
#include "RscpProtocol.h"
#include "RscpTags.h"
#include "SocketConnection.h"
#include "AES.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <cmath>

// RSCP connection state
int iSocket = -1;
int iAuthenticated = 0;
AES aesEncrypter;
AES aesDecrypter;
uint8_t ucEncryptionIV[AES_BLOCK_SIZE];
uint8_t ucDecryptionIV[AES_BLOCK_SIZE];

const char* getErrorDescription(uint32_t errorCode) {
    switch(errorCode) {
        case RSCP_ERR_NOT_HANDLED:
            return "Not handled - Request cannot be processed";
        case RSCP_ERR_ACCESS_DENIED:
            return "Access denied - Insufficient permissions";
        case RSCP_ERR_FORMAT:
            return "Format error - Invalid request format";
        case RSCP_ERR_AGAIN:
            return "Try again - Resource temporarily unavailable";
        case RSCP_ERR_OUT_OF_BOUNDS:
            return "Out of bounds - Value outside valid range";
        case RSCP_ERR_NOT_AVAILABLE:
            return "Not available - Requested data/feature not available";
        case RSCP_ERR_UNKNOWN_TAG:
            return "Unknown tag - Tag not supported by device";
        case RSCP_ERR_ALREADY_IN_USE:
            return "Already in use - Resource currently occupied";
        default:
            return "Unknown error";
    }
}

// Forward declarations for helper functions
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
                DEBUG("Sende TAG_EMS_REQ_START_MANUAL_CHARGE (0x%08X) mit Ladungsmenge: %u Wh\n", 
                      TAG_EMS_REQ_START_MANUAL_CHARGE, g_ctx.ladungsMenge);
                if (g_ctx.ladungsMenge == 0) {
                    DEBUG("  -> Anforderung: Manuelles Laden STOPPEN\n");
                } else {
                    DEBUG("  -> Anforderung: Manuelles Laden STARTEN mit %u Wh\n", g_ctx.ladungsMenge);
                }
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

        if (g_ctx.setEPReserve){
                DEBUG("Sende TAG_EP_REQ_SET_EP_RESERVE (0x%08X) mit Reserve: %.0f Wh\n", 
                      TAG_EP_REQ_SET_EP_RESERVE, g_ctx.epReserveWh);
                
                // Create EP_REQ_SET_EP_RESERVE container
                SRscpValue epContainer;
                protocol.createContainerValue(&epContainer, TAG_EP_REQ_SET_EP_RESERVE);
                
                // Add parameter index (always 0 for main parameter)
                protocol.appendValue(&epContainer, TAG_EP_PARAM_INDEX, (uint8_t)0);
                
                // Add reserve energy value in Wh
                protocol.appendValue(&epContainer, TAG_EP_PARAM_EP_RESERVE_ENERGY, g_ctx.epReserveWh);
                
                // Append container to root
                protocol.appendValue(&rootValue, epContainer);
                protocol.destroyValueData(epContainer);
                
                printf("Setze Notstromreserve auf %.0f Wh\n", g_ctx.epReserveWh);
        }

    }

    // create buffer frame to send data to the S10
    protocol.createFrameAsBuffer(frameBuffer, rootValue.data, rootValue.length, true); // true to calculate CRC on for transfer
    // the root value object should be destroyed after the data is copied into the frameBuffer and is not needed anymore
    protocol.destroyValueData(rootValue);

    return 0;
}

// Get tag description from loaded tags (search all categories)
int handleResponseValue(RscpProtocol *protocol, SRscpValue *response) {
    // check if any of the response has the error flag set and react accordingly
    if(response->dataType == RSCP::eTypeError) {
        // handle error for example access denied errors
        uint32_t uiErrorCode = protocol->getValueAsUInt32(response);
        const char* errorDesc = getErrorDescription(uiErrorCode);
        fprintf(stderr, "RSCP Error: Tag 0x%08X - Code 0x%02X (%u): %s\n", 
                response->tag, uiErrorCode, uiErrorCode, errorDesc);
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
        } else {
            fprintf(stderr, "FEHLER: Authentifizierung fehlgeschlagen - Zugriff verweigert\n");
            exit(EXIT_FAILURE);
        }
        DEBUG("RSCP authentitication level %i\n", ucAccessLevel);
        break;
    }
    case TAG_EMS_START_MANUAL_CHARGE: {
        DEBUG("Empfange TAG_EMS_START_MANUAL_CHARGE (0x%08X) Response\n", response->tag);
        DEBUG("  Response DataType: %d\n", response->dataType);
        
        bool result = protocol->getValueAsBool(response);
        DEBUG("  Response Wert (Bool): %s\n", result ? "true (akzeptiert)" : "false (abgelehnt)");
        
        if (result){
                if (g_ctx.ladungsMenge == 0){
                        printf("Manuelles Laden gestoppt\n");
                        DEBUG("  -> Erfolgreich: Ladevorgang wurde gestoppt\n");
                }else{
                        printf("Manuelles Laden gestartet\n");
                        DEBUG("  -> Erfolgreich: Ladevorgang mit %u Wh gestartet\n", g_ctx.ladungsMenge);
                }
        }else{
                printf("Manuelles Laden abgelehnt.\n");
                DEBUG("  -> ABGELEHNT: E3DC hat den Ladebefehl nicht akzeptiert!\n");
                DEBUG("     Mögliche Gründe:\n");
                DEBUG("     - Batterie bereits voll (SOC 100%%)\n");
                DEBUG("     - Anderer Ladevorgang aktiv\n");
                DEBUG("     - Netzstrom nicht verfügbar\n");
                DEBUG("     - Systemfehler am E3DC\n");
        }
        break;
    }
    case TAG_EP_EP_RESERVE:
    case TAG_EP_SET_EP_RESERVE: {
        DEBUG("Empfange EP Reserve Response (0x%08X)\n", response->tag);
        
        // Response is a container with the set values
        std::vector<SRscpValue> epData = protocol->getValueAsContainer(response);
        float reserveWh = 0.0f;
        float reservePercent = 0.0f;
        
        for(size_t i = 0; i < epData.size(); i++){
            switch(epData[i].tag){
                case TAG_EP_PARAM_EP_RESERVE_ENERGY:
                    reserveWh = protocol->getValueAsFloat32(&epData[i]);
                    break;
                case TAG_EP_PARAM_EP_RESERVE:
                    reservePercent = protocol->getValueAsFloat32(&epData[i]);
                    break;
            }
            protocol->destroyValueData(epData[i]);
        }
        
        printf("Notstromreserve gesetzt: %.0f Wh (%.1f%%)\n", reserveWh, reservePercent);
        break;
    }
    case TAG_EMS_POWER_PV: {    // response for TAG_EMS_REQ_POWER_PV
        int32_t iPower = protocol->getValueAsInt32(response);
        if (g_ctx.jsonOutput) {
            jsonStart();
            jsonFieldInt("tag", TAG_EMS_POWER_PV);
            jsonField("name", "EMS_POWER_PV");
            jsonFieldInt("value", iPower);
            jsonField("unit", "W");
            jsonEnd();
        } else if (g_ctx.quietMode) {
            printf("%i\n", iPower);
        } else {
            printf("EMS PV power is %i W\n", iPower);
        }
        break;
    }
    case TAG_EMS_POWER_BAT: {    // response for TAG_EMS_REQ_POWER_BAT
        int32_t iPower = protocol->getValueAsInt32(response);
        if (g_ctx.jsonOutput) {
            jsonStart();
            jsonFieldInt("tag", TAG_EMS_POWER_BAT);
            jsonField("name", "EMS_POWER_BAT");
            jsonFieldInt("value", iPower);
            jsonField("unit", "W");
            jsonEnd();
        } else if (g_ctx.quietMode) {
            printf("%i\n", iPower);
        } else {
            printf("EMS BAT power is %i W\n", iPower);
        }
        break;
    }
    case TAG_EMS_POWER_HOME: {    // response for TAG_EMS_REQ_POWER_HOME
        int32_t iPower = protocol->getValueAsInt32(response);
        if (g_ctx.jsonOutput) {
            jsonStart();
            jsonFieldInt("tag", TAG_EMS_POWER_HOME);
            jsonField("name", "EMS_POWER_HOME");
            jsonFieldInt("value", iPower);
            jsonField("unit", "W");
            jsonEnd();
        } else if (g_ctx.quietMode) {
            printf("%i\n", iPower);
        } else {
            printf("EMS house power is %i W\n", iPower);
        }
        break;
    }
    case TAG_EMS_POWER_GRID: {    // response for TAG_EMS_REQ_POWER_GRID
        int32_t iPower = protocol->getValueAsInt32(response);
        if (g_ctx.jsonOutput) {
            jsonStart();
            jsonFieldInt("tag", TAG_EMS_POWER_GRID);
            jsonField("name", "EMS_POWER_GRID");
            jsonFieldInt("value", iPower);
            jsonField("unit", "W");
            jsonEnd();
        } else if (g_ctx.quietMode) {
            printf("%i\n", iPower);
        } else {
            printf("EMS grid power is %i W\n", iPower);
        }
        break;
    }
    case TAG_EMS_POWER_ADD: {    // response for TAG_EMS_REQ_POWER_ADD
        int32_t iPower = protocol->getValueAsInt32(response);
        if (g_ctx.jsonOutput) {
            jsonStart();
            jsonFieldInt("tag", TAG_EMS_POWER_ADD);
            jsonField("name", "EMS_POWER_ADD");
            jsonFieldInt("value", iPower);
            jsonField("unit", "W");
            jsonEnd();
        } else if (g_ctx.quietMode) {
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
        
        // Print header for module info dump (only on first call for this module)
        if (g_ctx.modulInfoDump && !g_ctx.quietMode && g_ctx.isFirstModuleDumpRequest) {
            printf("Batterie Modul %u:\n", g_ctx.batIndex);
        }
        
        for(size_t i = 0; i < batteryData.size(); ++i) {
            // Check for errors first - stop processing if error found
            if(batteryData[i].dataType == RSCP::eTypeError) {
                uint32_t uiErrorCode = protocol->getValueAsUInt32(&batteryData[i]);
                const char* errorDesc = getErrorDescription(uiErrorCode);
                // Always output errors to stderr (quiet-mode contract)
                fprintf(stderr, "RSCP Error: Tag 0x%08X - Code 0x%02X (%u): %s\n", 
                        batteryData[i].tag, uiErrorCode, uiErrorCode, errorDesc);
                // Clean up vector elements
                for(size_t j = 0; j < batteryData.size(); ++j) {
                    protocol->destroyValueData(&batteryData[j]);
                }
                g_ctx.batContainerQuery = false;  // Reset flag
                return -1;  // Stop processing after error
            }
            
            // Skip BAT_INDEX in output (BAT_DCB_INFO is handled in container case)
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
                    g_ctx.dcbRequestRetries = 0;  // Reset retry counter
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
            // Skip printing BAT_DCB_INFO tag itself (only print its contents)
            if (!g_ctx.quietMode && batteryData[i].tag != TAG_BAT_DCB_INFO) {
                if (g_ctx.modulInfoDump) {
                    // Friendly label for module info dump
                    const char* label = getTagDescription(batteryData[i].tag);
                    if (label) {
                        printf("%s\n", label);
                    } else {
                        printf("Tag 0x%08X:\n", batteryData[i].tag);
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
                    } else if (g_ctx.modulInfoDump && interp) {
                        printf("  %s (%s)\n", buf, interp);
                    } else if (g_ctx.modulInfoDump) {
                        printf("  %s\n", buf);
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
                    if (g_ctx.quietMode) {
                        printf("%s\n", buf);
                    } else if (g_ctx.modulInfoDump) {
                        const char* interp = interpretValue(batteryData[i].tag, value);
                        if (interp) {
                            printf("  %s (%s)\n", buf, interp);
                        } else {
                            printf("  %s\n", buf);
                        }
                    } else {
                        printFormattedValue(batteryData[i].tag, buf, value);
                    }
                    break;
                }
                case RSCP::eTypeInt32: {
                    int32_t value = protocol->getValueAsInt32(&batteryData[i]);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%d", value);
                    if (g_ctx.quietMode) {
                        printf("%s\n", buf);
                    } else if (g_ctx.modulInfoDump) {
                        const char* interp = interpretValue(batteryData[i].tag, value);
                        if (interp) {
                            printf("  %s (%s)\n", buf, interp);
                        } else {
                            printf("  %s\n", buf);
                        }
                    } else {
                        printFormattedValue(batteryData[i].tag, buf, value);
                    }
                    break;
                }
                case RSCP::eTypeUInt32: {
                    uint32_t value = protocol->getValueAsUInt32(&batteryData[i]);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%u", value);
                    if (g_ctx.quietMode) {
                        printf("%s\n", buf);
                    } else if (g_ctx.modulInfoDump) {
                        const char* interp = interpretValue(batteryData[i].tag, value);
                        if (interp) {
                            printf("  %s (%s)\n", buf, interp);
                        } else {
                            printf("  %s\n", buf);
                        }
                    } else {
                        printFormattedValue(batteryData[i].tag, buf, value);
                    }
                    break;
                }
                case RSCP::eTypeString: {
                    std::string str = protocol->getValueAsString(&batteryData[i]);
                    if (g_ctx.modulInfoDump) {
                        printf("  %s\n", str.c_str());
                    } else {
                        printf("%s\n", str.c_str());
                    }
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
                            for (auto& dcbPair : dcbData) {
                                printf("Zellblock #%u\n", dcbPair.first);
                                for (auto& tagValuePair : dcbPair.second) {
                                    const char* label = getTagDescription(tagValuePair.first);
                                    if (label) {
                                        printf("%s\n", label);
                                    } else {
                                        printf("Tag 0x%08X:\n", tagValuePair.first);
                                    }
                                    
                                    // Formatiere Wert mit 2 Leerzeichen Abstand
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
        if (g_ctx.needMoreDCBRequests && g_ctx.modulInfoDump) {
            if (receivedDCBData) {
                // Success - received DCB data, reset retry counter
                g_ctx.dcbRequestRetries = 0;
                g_ctx.currentDCBIndex++;
                
                // Check if we've queried all DCBs
                if (g_ctx.currentDCBIndex >= g_ctx.totalDCBs) {
                    g_ctx.needMoreDCBRequests = false;
                    g_ctx.isFirstModuleDumpRequest = true;  // Reset for next dump
                }
            } else {
                // No DCB data received - increment retry counter
                g_ctx.dcbRequestRetries++;
                
                // Max retries per DCB index to prevent infinite loops (configurable via max_retries)
                if (g_ctx.dcbRequestRetries >= e3dc_config.max_retries) {
                    fprintf(stderr, "WARNING: DCB #%d failed after %u attempts - skipping\n",
                            g_ctx.currentDCBIndex, e3dc_config.max_retries);
                    g_ctx.dcbRequestRetries = 0;
                    g_ctx.currentDCBIndex++;  // Weiter mit nächstem DCB
                    if (g_ctx.currentDCBIndex >= g_ctx.totalDCBs) {
                        g_ctx.needMoreDCBRequests = false;
                    }
                }
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
                    const char* errorDesc = getErrorDescription(uiErrorCode);
                    fprintf(stderr, "RSCP Error: Tag 0x%08X - Code 0x%02X (%u): %s\n", 
                            PMData[i].tag, uiErrorCode, uiErrorCode, errorDesc);
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
        
        if (g_ctx.jsonOutput) {
            jsonStart();
            jsonField("type", typeStr);
            jsonField("start_date", startStr);
            jsonField("end_date", endStr);
            jsonField("interval", intervalName);
        } else {
            printf("Zeitraum: %s - %s\n", startStr, endStr);
        }
        
        std::vector<SRscpValue> historyData = protocol->getValueAsContainer(response);
        
        for(size_t i = 0; i < historyData.size(); ++i) {
            if(historyData[i].dataType == RSCP::eTypeError) {
                uint32_t uiErrorCode = protocol->getValueAsUInt32(&historyData[i]);
                const char* errorDesc = getErrorDescription(uiErrorCode);
                fprintf(stderr, "RSCP Error: Tag 0x%08X - Code 0x%02X (%u): %s\n", 
                        historyData[i].tag, uiErrorCode, uiErrorCode, errorDesc);
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
                    
                    if (g_ctx.jsonOutput) {
                        jsonFieldFloat("pv_production_kwh", dcPower / 1000.0);
                        jsonFieldFloat("battery_charge_kwh", batPowerIn / 1000.0);
                        jsonFieldFloat("battery_discharge_kwh", batPowerOut / 1000.0);
                        jsonFieldFloat("grid_import_kwh", gridPowerOut / 1000.0);
                        jsonFieldFloat("grid_export_kwh", gridPowerIn / 1000.0);
                        jsonFieldFloat("consumption_kwh", consumption / 1000.0);
                        if (autarky > 0) jsonFieldFloat("autarky_percent", autarky);
                    } else {
                        printf("PV-Produktion:      %.2f kWh\n", dcPower / 1000.0);
                        printf("Batterie geladen:   %.2f kWh\n", batPowerIn / 1000.0);
                        printf("Batterie entladen:  %.2f kWh\n", batPowerOut / 1000.0);
                        printf("Netzbezug:          %.2f kWh\n", gridPowerOut / 1000.0);
                        printf("Netzeinspeisung:    %.2f kWh\n", gridPowerIn / 1000.0);
                        printf("Hausverbrauch:      %.2f kWh\n", consumption / 1000.0);
                        if (autarky > 0) printf("Autarkie:           %.1f %%\n", autarky);
                    }
                    
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
        
        if (g_ctx.jsonOutput) {
            jsonEnd();
        }
        
        break;
    }
    
    // ...
    default:
        // Generic handler for read requests
        if (g_ctx.werteAbfragen) {
            if (g_ctx.jsonOutput) {
                jsonStart();
                jsonFieldInt("tag", response->tag);
                
                // Add tag name if available
                const char* tagDesc = getTagDescription(response->tag);
                if (tagDesc) {
                    jsonField("description", tagDesc);
                }
            } else if (!g_ctx.quietMode) {
                printf("Tag 0x%08X: ", response->tag);
            }
            
            switch(response->dataType) {
                case RSCP::eTypeBool: {
                    bool bValue = protocol->getValueAsBool(response);
                    if (g_ctx.jsonOutput) {
                        jsonField("type", "bool");
                        jsonField("value", bValue ? "true" : "false", false);
                        jsonEnd();
                    } else {
                        printFormattedValue(response->tag, bValue ? "true" : "false", bValue ? 1 : 0);
                    }
                    break;
                }
                case RSCP::eTypeChar8: {
                    int8_t value = protocol->getValueAsChar8(response);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%d", value);
                    if (g_ctx.jsonOutput) {
                        jsonField("type", "char8");
                        jsonFieldInt("value", value);
                        jsonEnd();
                    } else {
                        printFormattedValue(response->tag, buf, value);
                    }
                    break;
                }
                case RSCP::eTypeUChar8: {
                    uint8_t value = protocol->getValueAsUChar8(response);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%u", value);
                    if (g_ctx.jsonOutput) {
                        jsonField("type", "uchar8");
                        jsonFieldInt("value", value);
                        jsonEnd();
                    } else {
                        printFormattedValue(response->tag, buf, value);
                    }
                    break;
                }
                case RSCP::eTypeInt16: {
                    int16_t value = protocol->getValueAsInt16(response);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%d", value);
                    if (g_ctx.jsonOutput) {
                        jsonField("type", "int16");
                        jsonFieldInt("value", value);
                        jsonEnd();
                    } else {
                        printFormattedValue(response->tag, buf, value);
                    }
                    break;
                }
                case RSCP::eTypeUInt16: {
                    uint16_t value = protocol->getValueAsUInt16(response);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%u", value);
                    if (g_ctx.jsonOutput) {
                        jsonField("type", "uint16");
                        jsonFieldInt("value", value);
                        jsonEnd();
                    } else {
                        printFormattedValue(response->tag, buf, value);
                    }
                    break;
                }
                case RSCP::eTypeInt32: {
                    int32_t value = protocol->getValueAsInt32(response);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%d", value);
                    if (g_ctx.jsonOutput) {
                        jsonField("type", "int32");
                        jsonFieldInt("value", value);
                        jsonEnd();
                    } else {
                        printFormattedValue(response->tag, buf, value);
                    }
                    break;
                }
                case RSCP::eTypeUInt32: {
                    uint32_t value = protocol->getValueAsUInt32(response);
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%u", value);
                    if (g_ctx.jsonOutput) {
                        jsonField("type", "uint32");
                        jsonFieldInt("value", value);
                        jsonEnd();
                    } else {
                        printFormattedValue(response->tag, buf, value);
                    }
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
                    if (g_ctx.jsonOutput) {
                        jsonField("type", "float32");
                        jsonFieldFloat("value", value);
                        jsonEnd();
                    } else {
                        printFormattedValue(response->tag, buf, (int64_t)value);
                    }
                    break;
                }
                case RSCP::eTypeDouble64: {
                    double value = protocol->getValueAsDouble64(response);
                    if (g_ctx.jsonOutput) {
                        jsonField("type", "double64");
                        jsonFieldFloat("value", value);
                        jsonEnd();
                    } else {
                        printf("%.2f\n", value);
                    }
                    break;
                }
                case RSCP::eTypeBitfield: {
                    uint32_t value = protocol->getValueAsUInt32(response);
                    if (g_ctx.jsonOutput) {
                        jsonField("type", "bitfield");
                        char hexBuf[16];
                        snprintf(hexBuf, sizeof(hexBuf), "0x%08X", value);
                        jsonField("value", hexBuf);
                        jsonEnd();
                    } else {
                        printf("0x%08X\n", value);
                    }
                    break;
                }
                case RSCP::eTypeString: {
                    std::string str = protocol->getValueAsString(response);
                    if (g_ctx.jsonOutput) {
                        jsonField("type", "string");
                        jsonField("value", str.c_str());
                        jsonEnd();
                    } else {
                        printf("%s\n", str.c_str());
                    }
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
                        // Validate timestamp is in reasonable range (2000-2100)
                        if (ts.seconds > 946684800ULL && ts.seconds < 4102444800ULL) {
                            time_t seconds = (time_t)ts.seconds;
                            struct tm timeinfo;
                            localtime_r(&seconds, &timeinfo);
                            char timeStr[80];
                            strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", &timeinfo);
                            printf("%s.%03u\n", timeStr, ts.nanoseconds / 1000000);
                        } else {
                            printf("%lld.%09u (ungültiger Timestamp)\n", (long long)ts.seconds, ts.nanoseconds);
                        }
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

void mainLoop(void)
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


