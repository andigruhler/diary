#ifndef DIARY_UTILS_H
#define DIARY_UTILS_H

#include <stdlib.h>
#include <time.h>

typedef struct
{
    // Path that holds the journal text files
    char* dir;
    // Number of years to show before/after todays date
    int range;
    // 7 = Sunday, 1 = Monday, ..., 6 = Saturday
    int weekday;
    // 2020-12-31
    char* fmt;
    // Editor to open journal files with
    char* editor;
} config;

config CONFIG;

void get_date_str(const struct tm* date, char* date_str, size_t date_str_size, const char* fmt);

#endif
