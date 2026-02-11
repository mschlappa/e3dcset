#ifndef HISTORY_H
#define HISTORY_H

#include <stdint.h>
#include <time.h>
#include <string>

// History-Funktionen
int getDaysInMonth(int month, int year);
time_t dateToTimestamp(const char* dateStr, const char* historyType = "day");
std::string formatTimestamp(uint64_t milliseconds);

#endif // HISTORY_H
