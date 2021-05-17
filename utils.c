#include "utils.h"

char* extract_json_value(char* json, char* key, bool quoted) {
    // work on a copy of the json
    char* tok = (char *) malloc(strlen(json) * sizeof(char));
    strcpy(tok, json);

    tok = strtok(tok, " ");
    while (tok != NULL) {
        if (strstr(tok, key) != NULL) {
            tok = strtok(NULL, " "); // value
            break;
        }
        // key was not in this tok, advance tok
        tok = strtok(NULL, " ");
    }

    // remove quotes and comma or commma only
    if (quoted) {
        tok = strtok(tok, "\"");
    } else {
        tok = strtok(tok, ",");
    }
    return tok;
}

// Return expanded file path
char* expand_path(char* str) {
    char* res;
    wordexp_t str_wordexp;
    if ( wordexp( str, &str_wordexp, 0 ) == 0) {
        res = (char *) calloc(strlen(str_wordexp.we_wordv[0]) + 1, sizeof(char));
        strcpy(res, str_wordexp.we_wordv[0]);
    }
    wordfree(&str_wordexp);
    return res;
}

config CONFIG = {
    .range = 1,
    .weekday = 1,
    .fmt = "%Y-%m-%d",
    .editor = "",
    .google_tokenfile = GOOGLE_OAUTH_TOKEN_FILE,
    .google_clientid = GOOGLE_OAUTH_CLIENT_ID,
    .google_secretid = GOOGLE_OAUTH_CLIENT_SECRET,
    .google_calendar = ""
};

void get_date_str(const struct tm* date, char* date_str, size_t date_str_size, const char* fmt)
{
    strftime(date_str, date_str_size, fmt, date);
}
