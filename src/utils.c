#include "utils.h"

/* Update the header with the cursor date */
void update_date(WINDOW* header, struct tm* curs_date) {
    // TODO: dstr for strlen(CONFIG.format) > 16 ?
    char dstr[16];
    mktime(curs_date);
    strftime(dstr, sizeof dstr, CONFIG.fmt, curs_date);

    wclear(header);
    mvwaddstr(header, 0, 0, dstr);
    wrefresh(header);
}


char* extract_json_value(const char* json, char* key, bool quoted) {
    // work on a copy of the json
    char* jsoncp = (char*) malloc(strlen(json) * sizeof(char));
    strcpy(jsoncp, json);

    char* tok = strtok(jsoncp, " ");
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

    char* res = NULL;
    if (tok != NULL) {
        res = (char*) malloc(strlen(tok) * sizeof(char));
        strcpy(res, tok);
    }

    free(jsoncp);
    return res;
}

char* fold(const char* str) {
    // work on a copy of the str
    int strl = strlen(str);
    char* strcp = (char *) malloc(strl * sizeof(char));
    strcpy(strcp, str);

    if (strlen(strcp) <= 75) return strcp;

    char* buf = malloc(1);
    buf = '\0';
    size_t bufl = 0;

    // break lines after 75 chars
    for (char* i = strcp; i-strcp < strl; i += 75) {
        // split between any two characters by inserting a CRLF
        // immediately followed by a white space character
        fprintf(stderr, "next size: %li\n", bufl + 77);
        fprintf(stderr, "buf: %s\n", buf);
        buf = realloc(buf, bufl + 77);
        if (buf == NULL) {
            perror("realloc failed");
            return buf;
        }
        strncat(buf, i, 75);
        buf[i - strcp] = '\n';
        buf[i - strcp + 1] = ' ';
        bufl = strlen(buf);
    }

    free(strcp);
    return buf;
}

char* unfold(const char* str) {
    // work on a copy of the str
    char* strcp = (char *) malloc(strlen(str) * sizeof(char));
    strcpy(strcp, str);

    char* res = strtok(strcp, "\n");

    char* buf = malloc(strlen(res));
    strcpy(buf, res);

    regex_t re;
    regmatch_t pm[1];

    if (regcomp(&re, "^[^ \t]", 0) != 0) {
        perror("Failed to compile regex");
        return NULL;
    }

    while (res != NULL) {
        res = strtok(NULL, "\n");

        if (regexec(&re, res, 1, pm, 0) == 0) {
            // Stop unfolding if line does not start with white space/tab:
            // https://datatracker.ietf.org/doc/html/rfc2445#section-4.1
            break;
        }

        buf = realloc(buf, strlen(buf) + strlen(res));
        if (buf != NULL) {
            strcat(buf, res + 1);
        } else {
            perror("realloc failed");
            return NULL;
        }
    }

    regfree(&re);
    free(strcp);
    return buf;
}

char* extract_ical_field(const char* ics, char* key, bool multiline) {
    regex_t re;
    regmatch_t pm[1];
    char key_regex[strlen(key) + 1];
    sprintf(key_regex, "^%s", key);
    fprintf(stderr, "Key regex: %s\n", key_regex);

    if (regcomp(&re, key_regex, 0) != 0) {
        perror("Failed to compile regex");
        return NULL;
    }

    // work on a copy of the ical xml response
    char* field = (char *) malloc(strlen(ics) * sizeof(char));
    strcpy(field, ics);

    char* res = strtok(field, "\n");

    while (res != NULL) {
        if (regexec(&re, res, 1, pm, 0) == 0) {
            res = strstr(res, ":"); // value
            res++; // strip the ":"
            break;
        }
        // key not in this line, advance line
        res = strtok(NULL, "\n");
    }

    if (multiline) {
        res = unfold(ics + (res - field));
    }

    free(field);
    return res;
}

// Return expanded file path
char* expand_path(const char* str) {
    char* res;
    wordexp_t str_wordexp;
    if ( wordexp( str, &str_wordexp, 0 ) == 0) {
        res = (char *) calloc(strlen(str_wordexp.we_wordv[0]) + 1, sizeof(char));
        strcpy(res, str_wordexp.we_wordv[0]);
    }
    wordfree(&str_wordexp);
    return res;
}

// Get last occurence of string in string
// https://stackoverflow.com/questions/20213799/finding-last-occurence-of-string
char* strrstr(char *haystack, char *needle) {
    int nlen = strlen(needle);
    for (char* i = haystack + strlen(haystack) - nlen; i >= haystack; i--) {
        if (strncmp(i, needle, nlen) == 0) {
            return i;
        }
    }
    return NULL;
}

/* Writes file path for 'date' entry to 'rpath'. '*rpath' is NULL on error. */
void fpath(const char* dir, size_t dir_size, const struct tm* date, char** rpath, size_t rpath_size)
{
    // check size of result path
    if (dir_size + 1 > rpath_size) {
        fprintf(stderr, "Directory path too long");
        *rpath = NULL;
        return;
    }

    // add path of the diary dir to result path
    strcpy(*rpath, dir);

    // check for terminating '/' in path
    if (dir[dir_size - 1] != '/') {
        // check size again to accommodate '/'
        if (dir_size + 1 > rpath_size) {
            fprintf(stderr, "Directory path too long");
            *rpath = NULL;
            return;
        }
        strcat(*rpath, "/");
    }

    char dstr[16];
    strftime(dstr, sizeof dstr, CONFIG.fmt, date);

    // append date to the result path
    if (strlen(*rpath) + strlen(dstr) > rpath_size) {
        fprintf(stderr, "File path too long");
        *rpath = NULL;
        return;
    }
    strcat(*rpath, dstr);
}

// TODO: write functions for (un)escaped TEXT
// https://datatracker.ietf.org/doc/html/rfc5545#section-3.3.11
char* unescape_ical_description(const char* description);
char* escape_ical_description(const char* description);

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
