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

// Extract OAuth2 token from json response
char* extract_oauth_token(char* json) {
    char* res = (char *) malloc(strlen(json) * sizeof(char));
    strcpy(res, json);

    // example json:
    // {
    //   "access_token": "ya29.a0AfH6SMB1xpfVS6OdyRop3OjdetuwizbG1B83wRhQMxEym0fMf9HYwKBs_ulSgZSOMgH-FRcVGEGAnVPCaKjs0afExqAuy-aOQbBu2v4uu42V7Juwb11FDqftuVJpTjErVY_KWk1yG0EgzmVAlVZK8YsxQlPB",
    //   "expires_in": 3599,
    //   "refresh_token": "1//09hh-LM2w29NNCgYIARAAGAkSNwF-L9Ir_cOriqpjUHd97eiWNywBWjFiMRshfxlQFxpDIg8XqhK4OasuxGlro0r1XK1OuprSlNc",
    //   "scope": "https://www.googleapis.com/auth/calendar",
    //   "token_type": "Bearer"
    // }
    res = strtok(res, " ");
    while (res != NULL) {
        if (strstr(res, "access_token") != NULL) {
            res = strtok(NULL, " "); // token value
            res = strtok(res, "\"");
            break;
        }
        res = strtok(NULL, " ");
    }
    return res;
}

// Extract OAuth2 token ttl
int extract_oauth_token_ttl(char* json) {
    char* res = (char *) malloc(strlen(json) * sizeof(char));
    strcpy(res, json);

    res = strtok(res, " ");
    while (res != NULL) {
        if (strstr(res, "expires_in") != NULL) {
            res = strtok(NULL, " "); // ttl
            res = strtok(res, ",");
            break;
        }
        res = strtok(NULL, " ");
    }
    return atoi(res);
}

// Extract OAuth2 refresh token from json response
char* extract_oauth_refresh_token(char* json) {
    char* res = (char *) malloc(strlen(json) * sizeof(char));
    strcpy(res, json);

    res = strtok(json, " ");
    while (res != NULL) {
        if (strstr(res, "refresh_token") != NULL) {
            res = strtok(NULL, " "); // token value
            res = strtok(res, "\"");
            break;
        }
        res = strtok(NULL, " ");
    }
    return res;
}

// todo: refresh OAuth2 access token
char* refresh_access_token(char* refresh_token) {
    return "not implemented";
}

void read_tokenfile() {
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
    fseek(token_file, 0L, SEEK_SET);

    token_buff = (char*)calloc(token_bytes, sizeof(char));
    fread(token_buff, sizeof(char), token_bytes, token_file);
    fclose(token_file);

    access_token = extract_oauth_token(token_buff);
    token_ttl = extract_oauth_token_ttl(token_buff);
    refresh_token = extract_oauth_refresh_token(token_buff);
    fprintf(stderr, "Access token: %s\n", access_token);
    fprintf(stderr, "Token TTL: %i\n", token_ttl);
    fprintf(stderr, "Refresh token: %s\n", refresh_token);
}

void get_access_token_from_code(char* code, char* verifier) {
    CURLcode res;

    char postfields[500];
    sprintf(postfields, "client_id=%s&client_secret=%s&code=%s&code_verifier=%s&grant_type=authorization_code&redirect_uri=http://%s:%i",
            CONFIG.google_clientid,
            CONFIG.google_secretid,
            code,
            verifier,
            ipstr,
            GOOGLE_OAUTH_REDIRECT_PORT);
    fprintf(stderr, "CURLOPT_POSTFIELDS: %s\n", postfields);

    curl = curl_easy_init();

    // https://curl.se/libcurl/c/getinmemory.html
    //struct curl_mem_chunk token_result;
    //token_result.memory = malloc(1);
    //token_result.size = 0;

    FILE* tokenfile;
    char* tokenfile_path;

    if (curl) {
        curl_easy_setopt(curl, CURLOPT_URL, GOOGLE_OAUTH_TOKEN_URL);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, postfields);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
        //curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_mem_callback);
        //curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&token_result);

        tokenfile_path = expand_path(CONFIG.google_tokenfile);
        tokenfile = fopen(tokenfile_path, "wb");
        if (tokenfile == NULL) {
            perror("Failed to open tokenfile:");
            fprintf(stderr, "Tokenfile: %s\n", tokenfile_path);
        } else {
            curl_easy_setopt(curl, CURLOPT_WRITEDATA, tokenfile);
            res = curl_easy_perform(curl);
            fclose(tokenfile);
        }

        if (res != CURLE_OK) {
            fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
        }

        //fprintf(stderr, "Curl retrieved %lu bytes\n", (unsigned long)token_result.size);
        //fprintf(stderr, "Curl content: %s\n", token_result.memory);

        curl_easy_cleanup(curl);
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

void caldav_sync(struct tm* date, WINDOW* header) {
    // fetch existing API tokens
    read_tokenfile();

    // check if we can use the existing token

    // create new verifier
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
    get_access_token_from_code(code, challenge);

    char dstr[16];
    mktime(date);
    get_date_str(date, dstr, sizeof dstr, CONFIG.fmt);
    fprintf(stderr, "\nCursor date: %s\n\n", dstr);
}
