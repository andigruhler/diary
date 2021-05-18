#include "caldav.h"

CURL *curl;
char* access_token;
char* refresh_token;
int token_ttl;

// Local bind address for receiving OAuth callbacks.
// Reserve 2 chars for the ipv6 square brackets.
char ip[INET6_ADDRSTRLEN], ipstr[INET6_ADDRSTRLEN+2];

/* Write a random code challenge of size len to dest */
void random_code_challenge(size_t len, char* dest) {
    // https://developers.google.com/identity/protocols/oauth2/native-app#create-code-challenge
    // A code_verifier is a random string using characters [A-Z] / [a-z] / [0-9] / "-" / "." / "_" / "~"
    srand(time(NULL));

    char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
    size_t alphabet_size = strlen(alphabet);

    for (int i = 0; i < len; i++) {
        dest[i] = alphabet[rand() % alphabet_size];
    }
    dest[len-1] = '\0';
}

static size_t curl_write_mem_callback(void * contents, size_t size, size_t nmemb, void *userp) {
    size_t realsize = size * nmemb;
    struct curl_mem_chunk* mem = (struct curl_mem_chunk*)userp;

    char* ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        fprintf(stderr, "not enough memory (realloc in CURLOPT_WRITEFUNCTION returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

static size_t write_data(void *ptr, size_t size, size_t nmemb, void *stream) {
    size_t written = fwrite(ptr, size, nmemb, (FILE *)stream);
    return written;
}

// todo
// https://beej.us/guide/bgnet/html
void* get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

/*
* Extract OAuth2 code from http header. Reads the code from
* the http header in http_header and writes the OAuth2 code
* to the char string pointed to by code.
*/
char* extract_oauth_code(char* http_header) {
    // example token: "/?code=&scope="
    char* res = strtok(http_header, " ");
    while (res != NULL) {
        if (strstr(res, "code") != NULL) {
            res = strtok(res, "="); // code key
            res = strtok(NULL, "&"); // code value
            fprintf(stderr, "Code: %s\n", res);
            break;
        }
        res = strtok(NULL, " ");
    }
    return res;
}

char* read_tokenfile() {
    FILE* token_file;
    char* token_buff;
    long token_bytes;

    char* tokenfile_path = expand_path(CONFIG.google_tokenfile);
    token_file = fopen(tokenfile_path, "r");
    if (token_file == NULL) {
        perror("Failed to open tokenfile:");
    }

    fseek(token_file, 0, SEEK_END);
    token_bytes = ftell(token_file);
    rewind(token_file);

    token_buff = malloc(token_bytes);
    if (token_buff != NULL) {
        fread(token_buff, sizeof(char), token_bytes, token_file);

        access_token = extract_json_value(token_buff, "access_token", true);

        // our program segfaults if we supply a NULL value to atoi
        char* token_ttl_str = extract_json_value(token_buff, "expires_in", false);
        if (token_ttl_str == NULL) {
            token_ttl = 0;
        } else {
            token_ttl = atoi(token_ttl_str);
        }

        // only update the existing refresh token if the request actually
        // contained a valid refresh_token, i.e, if it was the initial
        // interactive authZ request from token code confirmed by the user
        char * new_refresh_token = extract_json_value(token_buff, "refresh_token", true);
        if (new_refresh_token != NULL) {
            refresh_token = new_refresh_token;
        }

        fprintf(stderr, "Access token: %s\n", access_token);
        fprintf(stderr, "Token TTL: %i\n", token_ttl);
        fprintf(stderr, "Refresh token: %s\n", refresh_token);
    }
    fclose(token_file);
    return token_buff;
}

void write_tokenfile() {
    char* tokenfile_path = expand_path(CONFIG.google_tokenfile);
    FILE* tokenfile = fopen(tokenfile_path, "wb");
    if (tokenfile == NULL) {
        perror("Failed to open tokenfile:");
    } else {
        char contents[1000];
        char* tokenfile_contents = "{\n"
        "  \"access_token\": \"%s\",\n"
        "  \"expires_in\": %i,\n"
        "  \"refresh_token\": \"%s\"\n"
        "}\n";
        sprintf(contents, tokenfile_contents,
                access_token,
                token_ttl,
                refresh_token);
        fprintf(tokenfile, contents);
    }
    fclose(tokenfile);
    char* token_json = read_tokenfile();
    fprintf(stderr, "New tokenfile contents: %s\n", token_json);
    fprintf(stderr, "New Access token: %s\n", access_token);
    fprintf(stderr, "New Token TTL: %i\n", token_ttl);
    fprintf(stderr, "Refresh token: %s\n", refresh_token);
}

void get_access_token(char* code, char* verifier, bool refresh) {
    CURLcode res;

    char postfields[500];
    if (refresh) {
        sprintf(postfields, "client_id=%s&client_secret=%s&grant_type=refresh_token&refresh_token=%s",
                CONFIG.google_clientid,
                CONFIG.google_secretid,
                refresh_token);
    } else {
        sprintf(postfields, "client_id=%s&client_secret=%s&code=%s&code_verifier=%s&grant_type=authorization_code&redirect_uri=http://%s:%i",
                CONFIG.google_clientid,
                CONFIG.google_secretid,
                code,
                verifier,
                ipstr,
                GOOGLE_OAUTH_REDIRECT_PORT);
    }
    fprintf(stderr, "CURLOPT_POSTFIELDS: %s\n", postfields);

    curl = curl_easy_init();

    FILE* tokenfile;
    char* tokenfile_path;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, GOOGLE_OAUTH_TOKEN_URL);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);

        tokenfile_path = expand_path(CONFIG.google_tokenfile);
        tokenfile = fopen(tokenfile_path, "wb");
        if (tokenfile == NULL) {
            perror("Failed to open tokenfile:");
        } else {
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, tokenfile);
            res = curl_easy_perform(curl);
            fclose(tokenfile);
        }

        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            return;
        } else if (refresh) {
            // update global variables from tokenfile
            read_tokenfile();
            // Make sure the refresh token is re-written and persistet
            // to the tokenfile for further requests, becaues the
            // is not returned by the refresh_token call:
            // https://developers.google.com/identity/protocols/oauth2/native-app#offline
            write_tokenfile();
        }
    }

}

char* get_oauth_code(const char* verifier, WINDOW* header) {
    struct addrinfo hints, *addr_res;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int status;
    if ((status=getaddrinfo(NULL, MKSTR(GOOGLE_OAUTH_REDIRECT_PORT), &hints, &addr_res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    }

    void *addr;
    char *ipver;
    //todo: extract
    //addr = get_in_addr(addr_res->ai_addr);
    if (addr_res->ai_family == AF_INET) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *) addr_res->ai_addr;
        addr = &(ipv4->sin_addr);
        ipver = "IPv4";
    } else { // IPv6
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *) addr_res->ai_addr;
        addr = &(ipv6->sin6_addr);
        ipver = "IPv6";
    }

    inet_ntop(addr_res->ai_family, addr, ip, sizeof ip);
    if (strcmp("IPv6", ipver) == 0) {
        sprintf(ipstr, "[%s]", ip);
    }

    // Show Google OAuth URI
    char uri[500];
    sprintf(uri, "%s?scope=%s&code_challenge=%s&response_type=%s&redirect_uri=http://%s:%i&client_id=%s",
            GOOGLE_OAUTH_AUTHZ_URL,
            GOOGLE_OAUTH_SCOPE,
            verifier,
            GOOGLE_OAUTH_RESPONSE_TYPE,
            ipstr,
            GOOGLE_OAUTH_REDIRECT_PORT,
            CONFIG.google_clientid);
    fprintf(stderr, "Google OAuth2 authorization URI: %s\n", uri);

    // Show the Google OAuth2 authorization URI in the header
    wclear(header);
    int col;
    col = getmaxx(header);
    wresize(header, LINES, col);
    mvwprintw(header, 0, 0, "Go to Google OAuth2 authorization URI. Use 'q' or 'Ctrl+c' to quit authorization process.\n%s", uri);
    wrefresh(header);

    int socketfd = socket(addr_res->ai_family, addr_res->ai_socktype, addr_res->ai_protocol);
    if (socketfd < 0) {
       perror("Error opening socket");
    }

    // reuse socket address
    int yes=1;
    setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    if (bind(socketfd, addr_res->ai_addr, addr_res->ai_addrlen) < 0) {
       perror("Error binding socket");
    }

    freeaddrinfo(addr_res);

    int ls = listen(socketfd, GOOGLE_OAUTH_REDIRECT_SOCKET_BACKLOG);
    if (ls < 0) {
       perror("Listen error");
    }

    struct pollfd pfds[2];

    pfds[0].fd = STDIN_FILENO;
    pfds[0].events = POLLIN;
    pfds[1].fd = socketfd;
    pfds[1].events = POLLIN;
    int fd_count = 2;
            
    int connfd, bytes_rec, bytes_sent;
    char http_header[8*1024];
    char* reply =
    "HTTP/1.1 200 OK\n"
    "Content-Type: text/html\n"
    "Connection: close\n\n"
    "<html>"
    "<head><title>Authorization successfull</title></head>"
    "<body>"
    "<p><b>Authorization successfull.</b></p>"
    "<p>You consented that diary can access your Google calendar.<br/>"
    "Pleasee close this window and return to diary.</p>"
    "</body>"
    "</html>";

    // Handle descriptors read-to-read (POLLIN),
    // stdin or server socker, whichever is first
    for (;;) {
        int poll_count = poll(pfds, fd_count, -1);

        if (poll_count == -1) {
            perror("poll");
            break;
        }

        // Cancel through stdin
        if (pfds[0].revents & POLLIN) {
            int ch = getchar();
            // sudo showkey -a 
            // Ctrl+c: ^C 0x03
            // q     :  q 0x71
            if (ch == 0x03 || ch == 0x71) {
                fprintf(stderr, "Escape char: %x\n", ch);
                fprintf(stderr, "Hanging up, closing server socket\n");
                break;
            }
        }
        if (pfds[1].revents & POLLIN) {
            // accept connections but ignore client addr
            connfd = accept(socketfd, NULL, NULL);
            if (connfd < 0) {
               perror("Error accepting connection");
               break;
            }

            bytes_rec = recv(connfd, http_header, sizeof http_header, 0);
            if (bytes_rec < 0) {
                perror("Error reading stream message");
                break;
            }
            fprintf(stderr, "Received http header: %s\n", http_header);

            bytes_sent = send(connfd, reply, strlen(reply), 0);
            if (bytes_sent < 0) {
               perror("Error sending");
            }
            fprintf(stderr, "Bytes sent: %i\n", bytes_sent);

            close(connfd);
            break;
        }
    } // end for ;;

    // close server socket
    close(pfds[1].fd);

    char* code = extract_oauth_code(http_header);
    if (code == NULL) {
        fprintf(stderr, "Found no OAuth code in http header.\n");
        return NULL;
    }
    fprintf(stderr, "OAuth code: %s\n", code);

    return code;
}

char* caldav_req(struct tm* date, char* url, char* postfields, int depth) {

    // only support depths 0 or 1
    if (depth < 0 || depth > 1) {
        return NULL;
    }

    CURLcode res;

    curl = curl_easy_init();

    // https://curl.se/libcurl/c/getinmemory.html
    struct curl_mem_chunk caldav_resp;
    caldav_resp.memory = malloc(1);
    caldav_resp.size = 0;

    if (curl) {
        // fail if not authenticated, !CURLE_OK
        curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "PROPFIND");
        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_mem_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&caldav_resp);

        // construct header fields
        struct curl_slist *header = NULL;
        char bearer_token[strlen("Authorization: Bearer")+strlen(access_token)];
        sprintf(bearer_token, "Authorization: Bearer %s", access_token);
        char depth_header[strlen("Depth: 0")];
        sprintf(depth_header, "Depth: %i", depth);
        header = curl_slist_append(header, depth_header);
        header = curl_slist_append(header, bearer_token);
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, header);

        // set postfields, if any
        if (postfields != NULL) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
        }

        res = curl_easy_perform(curl);

        curl_easy_cleanup(curl);

        //fprintf(stderr, "Curl retrieved %lu bytes\n", (unsigned long)caldav_resp.size);
        //fprintf(stderr, "Curl content: %s\n", caldav_resp.memory);

        if (res != CURLE_OK) {
            //fprintf(stderr, "Curl response: %s\n", caldav_resp.memory);
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
            return NULL;
        }
    }

    return caldav_resp.memory;
}

// return current user principal from CalDAV XML response
char* parse_caldav_current_user_principal(char* xml) {
    char* xml_key_pos = strstr(xml, "<D:current-user-principal>");
    // this XML does not contain a user principal at all
    if (xml_key_pos == NULL) {
        return NULL;
    }

    //fprintf(stderr, "Found current-user-principal at position: %i\n", *xml_key_pos);

    //<D:current-user-principal>
    //<D:href>/caldav/v2/diary.in0rdr%40gmail.com/user</D:href>
    char* tok = strtok(xml_key_pos, "<"); // D:current-user-principal>
    if (tok != NULL) {
        tok = strtok(NULL, "<"); // D:href>/caldav/v2/test%40gmail.com/user
        fprintf(stderr, "First token: %s\n", tok);
        tok = strstr(tok, ">"); // >/caldav/v2/test%40gmail.com/user
        tok++; // cut >
        char* tok_end = strrchr(tok, '/');
        *tok_end = '\0'; // cut /user
    }

    return tok;
}

// return calendar uri from CalDAV home set XML response
char* parse_caldav_calendar(char* xml, char* calendar) {
    char displayname_needle[strlen(calendar) + strlen("<D:displayname></D:displayname>")];
    sprintf(displayname_needle, "<D:displayname>%s</D:displayname>", calendar);
    fprintf(stderr, "Displayname needle: %s\n", displayname_needle);
    char* displayname_pos = strstr(xml, displayname_needle);
    // this XML multistatus response does not contain the users calendar
    if (displayname_pos == NULL) {
        return NULL;
    }

    // <D:response>
    //  <D:href>/caldav/v2/2fcv7j5mf38o5u2kg5tar4baao%40group.calendar.google.com/events/</D:href>
    //  <D:propstat>
    //   <D:status></D:status>
    //   <D:prop>
    //    <D:displayname>diary</D:displayname>
    //   </D:prop>
    //  </D:propstat>
    // </D:response>

    // shorten multistatus response and find last hyperlink
    *displayname_pos= '\0';
    char* href = strrstr(xml, "<D:href>");
    if (href != NULL) {
        //fprintf(stderr, "Found calendar href: %s\n", href);
        href = strtok(href, "<"); // :href>/caldav/v2/aaa%40group.calendar.google.com/events/
        if (href != NULL) {
            href = strchr(href, '>');
            href++; // cut >
            //fprintf(stderr, "Found calendar href: %s\n", href);
        }
        return href;
    }
    return NULL;
}

void put_event(struct tm* date) {

}

void caldav_sync(struct tm* date, WINDOW* header) {
    // fetch existing API tokens
    read_tokenfile();

    if (access_token == NULL || refresh_token == NULL) {
        // no access token exists yet, create new verifier
        char challenge[GOOGLE_OAUTH_CODE_VERIFIER_LENGTH];
        random_code_challenge(GOOGLE_OAUTH_CODE_VERIFIER_LENGTH, challenge);
        fprintf(stderr, "Challenge/Verifier: %s\n", challenge);

        // fetch new code with verifier
        char* code = get_oauth_code(challenge, header);
        if (code == NULL) {
            fprintf(stderr, "Error retrieving access code.\n");
            return;
        }

        // get acess token using code and verifier
        get_access_token(code, challenge, false);
    }

    char* principal_postfields = "<d:propfind xmlns:d='DAV:' xmlns:cs='http://calendarserver.org/ns/'>"
                                "<d:prop><d:current-user-principal/></d:prop>"
                                "</d:propfind>";


    // check if we can use the token from the tokenfile
    char* user_principal = caldav_req(date, GOOGLE_CALDAV_URI, principal_postfields, 0);

    if (user_principal == NULL) {
        fprintf(stderr, "Unable to fetch principal, refreshing API token.\n");
        // The principal could not be fetched,
        // get new acess token with refresh token
        get_access_token(NULL, NULL, true);
        // Retry request for event with new token
        user_principal = caldav_req(date, GOOGLE_CALDAV_URI, principal_postfields, 0);
    }

    //fprintf(stderr, "\nUser Principal: %s\n\n", user_principal);
    user_principal = parse_caldav_current_user_principal(user_principal);
    fprintf(stderr, "\nUser Principal: %s\n\n", user_principal);

    // get the home-set of the user
    char uri[300];
    sprintf(uri, "%s%s", GOOGLE_API_URI, user_principal);
    fprintf(stderr, "\nHome Set URI: %s\n\n", uri);
    char* home_set = caldav_req(date, uri, "", 1);
    fprintf(stderr, "\nHome Set: %s\n\n", home_set);

    // get calendar URI from the home-set
    char* calendar = parse_caldav_calendar(home_set, CONFIG.google_calendar);
    fprintf(stderr, "\nCalendar href: %s\n\n", calendar);

    // get cursor date
    char dstr[16];
    mktime(date);
    get_date_str(date, dstr, sizeof dstr, CONFIG.fmt);
    fprintf(stderr, "\nCursor date: %s\n\n", dstr);

    // fetch event for the cursor date

    // check LAST-MODIFIED
    // if local file mod time more recent than LAST-MODIFIED
    //put_event(date);
    // else persist downloaded buffer to local file
}
