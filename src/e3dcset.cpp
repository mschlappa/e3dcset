#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <getopt.h>
#include "config.h"
#include "constants.h"
#include "rscp_handler.h"
#include "output.h"
#include "SocketConnection.h"

// Global flag for watch mode interruption
volatile sig_atomic_t g_watchInterrupted = 0;

// Signal handler for watch mode (Ctrl+C)
void watchSignalHandler(int signum) {
    (void)signum; // Unused parameter
    g_watchInterrupted = 1;
}

int main(int argc, char *argv[])
{
    if (argc == 1){
        usage();
    }
    
    int opt;
    int option_index = 0;
    
    static struct option long_options[] = {
        {"watch", no_argument, 0, 'w'},
        {"interval", required_argument, 0, 1},
        {0, 0, 0, 0}
    };

    while ((opt = getopt_long(argc, argv, "c:d:e:E:ap:r:i:m:qjlt:H:D:I:S:w", long_options, &option_index)) != -1) {
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
        case 'E':
            g_ctx.setEPReserve = true;
            g_ctx.epReserveWh = (float)atof(optarg);
            break;
        case 'a':
            g_ctx.leistungAendern = true;
            g_ctx.automatischLeistungEinstellen = true;
            break;
        case 'p':
            free(g_ctx.configPath);
            g_ctx.configPath = safe_strdup(optarg, "config path (-p)");
            break;
        case 't':
            free(g_ctx.tagfilePath);
            g_ctx.tagfilePath = safe_strdup(optarg, "tagfile path (-t)");
            break;
        case 'H':
            g_ctx.historieAbfrage = true;
            free(g_ctx.historieTyp);
            g_ctx.historieTyp = safe_strdup(optarg, "history type (-H)");
            if (strcmp(optarg, "day") != 0 && strcmp(optarg, "week") != 0 &&
                strcmp(optarg, "month") != 0 && strcmp(optarg, "year") != 0) {
                fprintf(stderr, "Fehler: Ungültiger History-Typ '%s'\n", optarg);
                fprintf(stderr, "Gültige Typen: day, week, month, year\n");
                exit(EXIT_FAILURE);
            }
            break;
        case 'D':
            free(g_ctx.historieDatum);
            g_ctx.historieDatum = safe_strdup(optarg, "history date (-D)");
            break;
        case 'r':
            g_ctx.werteAbfragen = true;
            if (optarg[0] >= '0' && optarg[0] <= '9') {
                g_ctx.leseTag = strtoul(optarg, NULL, 0);
                if (!isRequestTag(g_ctx.leseTag)) {
                    fprintf(stderr, "Fehler: 0x%08X ist ein RESPONSE Tag!\n", g_ctx.leseTag);
                    fprintf(stderr, "Sie können nur REQUEST Tags abfragen (zweites Byte < 0x80).\n");
                    fprintf(stderr, "Beispiel: 0x01000008 (REQUEST), nicht 0x01800008 (RESPONSE)\n");
                    exit(EXIT_FAILURE);
                }
            } else {
                free(g_ctx.tagName);
                g_ctx.tagName = safe_strdup(optarg, "tag name (-r)");
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
        case 'j':
            g_ctx.jsonOutput = true;
            break;
        case 'l':
            g_ctx.listTags = true;
            if (optind < argc && argv[optind][0] >= '0' && argv[optind][0] <= '9') {
                g_ctx.listCategory = atoi(argv[optind]);
                optind++;
                if (g_ctx.listCategory < MIN_TAG_CATEGORY || g_ctx.listCategory > MAX_TAG_CATEGORY) {
                    fprintf(stderr, "Fehler: Ungültige Kategorie %d (gültig: %d-%d)\n", 
                            g_ctx.listCategory, MIN_TAG_CATEGORY, MAX_TAG_CATEGORY);
                    fprintf(stderr, "Beispiel: ./e3dcset -l    (Übersicht)\n");
                    fprintf(stderr, "         ./e3dcset -l 1  (EMS Tags)\n\n");
                    usage();
                }
            } else {
                g_ctx.listCategory = 0;
            }
            break;
        case 'w':
            g_ctx.watchMode = true;
            break;
        case 1: // --interval
            g_ctx.watchInterval = (uint32_t)atoi(optarg);
            if (g_ctx.watchInterval < MIN_WATCH_INTERVAL_SECONDS) {
                fprintf(stderr, "Fehler: Interval muss mindestens %d Sekunde sein\n", 
                        MIN_WATCH_INTERVAL_SECONDS);
                exit(EXIT_FAILURE);
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

    // Register signal handler for watch mode (Ctrl+C)
    if (g_ctx.watchMode) {
        struct sigaction sa;
        sa.sa_handler = watchSignalHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, NULL);
    }

    // Verbinde mit Hauskraftwerk
    connectToServer();

    // Starte Sende- / Empfangsschleife
    mainLoop();

    // Trenne Verbindung zum Hauskraftwerk
    SocketClose(iSocket);
    
    DEBUG("Ende!\n\n");

    return 0;
}
