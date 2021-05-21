#ifndef DIARY_UTILS_H
#define DIARY_UTILS_H

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <wordexp.h>
#include <stdbool.h>

#define GOOGLE_OAUTH_TOKEN_FILE "~/.diary-token"
#ifndef GOOGLE_OAUTH_CLIENT_ID
    #define GOOGLE_OAUTH_CLIENT_ID ""
#endif
#ifndef GOOGLE_OAUTH_CLIENT_SECRET
    #define GOOGLE_OAUTH_CLIENT_SECRET ""
#endif

char* extract_json_value(char* json, char* key, bool quoted);
char* expand_path(char* str);
char* strrstr(char *haystack, char *needle);
void fpath(const char* dir, size_t dir_size, const struct tm* date, char** rpath, size_t rpath_size);

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
    // File for Google OAuth access token
    char* google_tokenfile;
    // Google client id
    char* google_clientid;
    // Google secret id
    char* google_secretid;
    // Google calendar to synchronize
    char* google_calendar;
} config;

config CONFIG;

#endif
