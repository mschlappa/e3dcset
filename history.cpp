#include "history.h"
#include "config.h"
#include "rscp_handler.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Berechnet Tage im Monat (unter Berücksichtigung von Schaltjahren)
int getDaysInMonth(int month, int year) {
    int daysInMonth[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    
    if (month < 1 || month > 12) return 31;
    
    int days = daysInMonth[month - 1];
    
    // Schaltjahr-Check für Februar
    if (month == 2) {
        if ((year % 4 == 0 && year % 100 != 0) || (year % 400 == 0)) {
            days = 29;
        }
    }
    
    return days;
}

// Konvertiert Datum zu Unix-Timestamp mit History-Typ-Anpassung
time_t dateToTimestamp(const char* dateStr, const char* historyType) {
    struct tm tm_date = {0};
    time_t now;
    int year = 0, month = 0;
    
    if (strcmp(dateStr, "today") == 0) {
        time(&now);
        struct tm* tm_now = localtime(&now);
        tm_date = *tm_now;
        year = tm_now->tm_year + 1900;
        month = tm_now->tm_mon + 1;
    } else {
        int day;
        if (sscanf(dateStr, "%d-%d-%d", &year, &month, &day) != 3) {
            fprintf(stderr, "Fehler: Ungültiges Datumsformat '%s'\n", dateStr);
            fprintf(stderr, "Verwenden Sie 'today' oder 'YYYY-MM-DD' (z.B. 2024-11-20)\n");
            exit(EXIT_FAILURE);
        }
        
        if (year < 1970 || year > 2100) {
            fprintf(stderr, "Fehler: Jahr muss zwischen 1970 und 2100 liegen (gelesen: %d)\n", year);
            exit(EXIT_FAILURE);
        }
        
        if (month < 1 || month > 12) {
            fprintf(stderr, "Fehler: Monat muss zwischen 1 und 12 liegen (gelesen: %d)\n", month);
            exit(EXIT_FAILURE);
        }
        
        int maxDay = getDaysInMonth(month, year);
        if (day < 1 || day > maxDay) {
            fprintf(stderr, "Fehler: Tag muss zwischen 1 und %d liegen für %d-%02d (gelesen: %d)\n",
                    maxDay, year, month, day);
            if (month == 2 && day == 29) {
                fprintf(stderr, "Hinweis: %d ist kein Schaltjahr (Februar hat nur 28 Tage)\n", year);
            }
            exit(EXIT_FAILURE);
        }
        
        tm_date.tm_year = year - 1900;
        tm_date.tm_mon = month - 1;
        tm_date.tm_mday = day;
        tm_date.tm_isdst = -1;
    }
    
    tm_date.tm_hour = 0;
    tm_date.tm_min = 0;
    tm_date.tm_sec = 0;
    
    // Anpassung basierend auf History-Typ
    if (strcmp(historyType, "week") == 0) {
        time_t temp = mktime(&tm_date);
        struct tm* pTemp = localtime(&temp);
        if (pTemp) {
            int daysToMonday = (pTemp->tm_wday == 0) ? 6 : pTemp->tm_wday - 1;
            tm_date.tm_mday -= daysToMonday;
        }
    } else if (strcmp(historyType, "month") == 0) {
        tm_date.tm_mday = 1;
        int daysInMonth = getDaysInMonth(month, year);
        extern CommandContext g_ctx;
        g_ctx.historieSpan = daysInMonth * 86400;
        DEBUG("Monat %d/%d hat %d Tage, SPAN = %u Sekunden\n", month, year, daysInMonth, g_ctx.historieSpan);
    } else if (strcmp(historyType, "year") == 0) {
        tm_date.tm_mon = 0;
        tm_date.tm_mday = 1;
    }
    
    time_t timestamp = mktime(&tm_date);
    if (timestamp == -1) {
        fprintf(stderr, "Fehler: Konnte Datum nicht konvertieren\n");
        exit(EXIT_FAILURE);
    }
    
    DEBUG("Konvertiere Datum '%s' (Typ: %s) zu Timestamp: %ld\n", dateStr, historyType, timestamp);
    return timestamp;
}

// Format millisecond Unix epoch timestamp to human-readable string
std::string formatTimestamp(uint64_t milliseconds) {
    // Validate timestamp range (1970-01-01 to 2100-12-31)
    // 0ms = 1970-01-01, 4133894400000ms = 2100-12-31 23:59:59
    const uint64_t MIN_TIMESTAMP_MS = 0;
    const uint64_t MAX_TIMESTAMP_MS = 4133894400000ULL;
    
    if (milliseconds > MAX_TIMESTAMP_MS) {
        fprintf(stderr, "Warning: Timestamp %llu ms out of valid range (max 2100-12-31), using fallback\n", 
                (unsigned long long)milliseconds);
        return "Invalid timestamp";
    }
    
    time_t seconds = milliseconds / 1000;
    struct tm timeinfo;
    
    if (localtime_r(&seconds, &timeinfo) == NULL) {
        fprintf(stderr, "Error: Failed to convert timestamp %llu ms to local time\n", 
                (unsigned long long)milliseconds);
        return "Invalid timestamp";
    }
    
    char buffer[64];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
    
    return std::string(buffer);
}
