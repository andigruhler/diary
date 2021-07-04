#ifndef DIARY_CALDAV_H
#define DIARY_CALDAV_H

#define __USE_XOPEN
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <netdb.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <ncurses.h>
#include <curl/curl.h>
#include "utils.h"

#define STR(s) #s
#define MKSTR(s) STR(s)

// https://developers.google.com/identity/protocols/oauth2/native-app#create-code-challenge
// A valid code_verifier has a length between 43 and 128 characters
#define GOOGLE_OAUTH_CODE_VERIFIER_LENGTH 43
#define GOOGLE_OAUTH_AUTHZ_URL "https://accounts.google.com/o/oauth2/auth"
#define GOOGLE_OAUTH_TOKEN_URL "https://oauth2.googleapis.com/token"
#define GOOGLE_OAUTH_SCOPE "https://www.googleapis.com/auth/calendar%20https://www.googleapis.com/auth/calendar.events.owned"
#define GOOGLE_OAUTH_RESPONSE_TYPE "code"
#define GOOGLE_OAUTH_REDIRECT_HOST "127.0.0.1"
#define GOOGLE_OAUTH_REDIRECT_PORT 9004
#define GOOGLE_OAUTH_REDIRECT_URI "http://" GOOGLE_OAUTH_REDIRECT_HOST ":" MKSTR(GOOGLE_OAUTH_REDIRECT_PORT)
#define GOOGLE_OAUTH_REDIRECT_SOCKET_BACKLOG 10
#define GOOGLE_API_URI "https://apidata.googleusercontent.com"
#define GOOGLE_CALDAV_URI GOOGLE_API_URI "/caldav/v2"

int caldav_sync(struct tm* date,
                 WINDOW* header,
                 WINDOW* cal,
                 int pad_pos,
                 const char* dir,
                 size_t dir_size,
                 bool confirm);

struct curl_mem_chunk {
    char* memory;
    size_t size;
};

extern config CONFIG;

#endif
