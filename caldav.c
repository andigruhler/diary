#include "caldav.h"

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

void* get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void caldav_sync(const struct tm* date, WINDOW* header) {
    char challenge[GOOGLE_OAUTH_CODE_VERIFIER_LENGTH];
    random_code_challenge(GOOGLE_OAUTH_CODE_VERIFIER_LENGTH, challenge);

    struct addrinfo hints, *res;

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    int status;
    if ((status=getaddrinfo(NULL, MKSTR(GOOGLE_OAUTH_REDIRECT_PORT), &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
    }

    void *addr;
    char *ipver;
    //todo: extract
    //addr = get_in_addr(res->ai_addr);
    if (res->ai_family == AF_INET) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in *) res->ai_addr;
        addr = &(ipv4->sin_addr);
        ipver = "IPv4";
    } else { // IPv6
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6 *) res->ai_addr;
        addr = &(ipv6->sin6_addr);
        ipver = "IPv6";
    }

    // Reserve 2 chars for the ipv6 square brackets
    char ip[INET6_ADDRSTRLEN], ipstr[INET6_ADDRSTRLEN+2];
    inet_ntop(res->ai_family, addr, ip, sizeof ip);

    if (strcmp("IPv6", ipver) == 0) {
        sprintf(ipstr, "[%s]", ip);
    }

    // Show Google Oauth URI
    char uri[250];
    sprintf(uri, "%s?scope=%s&response_type=%s&redirect_uri=http://%s:%i&client_id=%s",
            GOOGLE_OAUTH_AUTHZ_URL,
            GOOGLE_OAUTH_SCOPE,
            GOOGLE_OAUTH_RESPONSE_TYPE,
            ipstr,
            GOOGLE_OAUTH_REDIRECT_PORT,
            GOOGLE_OAUTH_CLIENT_ID);
    //fprintf(stderr, "Google OAuth2 authorization URI: %s\n", uri);

    // Show the Google OAuth2 authorization URI in the header
    wclear(header);
    int row, col;
    getmaxyx(header, row, col);
    wresize(header, row+5, col);
    //curs_set(2);
    mvwprintw(header, 0, 0, "Go to Google OAuth2 authorization URI. Use 'q' or 'Ctrl+c' to quit authorization process.\n%s", uri);

    //curs_set(0);
    wrefresh(header);

    int socketfd = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
    if (socketfd < 0) {
       perror("Error opening socket");
    }

    // reuse socket address
    int yes=1;
    setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof yes);

    if (bind(socketfd, res->ai_addr, res->ai_addrlen) < 0) {
       perror("Error binding socket");
    }

    freeaddrinfo(res);

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
    char* reply =
    "HTTP/1.1 200 OK\n"
    "Content-Type: text/html\n"
    "Connection: close\n\n"
    "<p><b>Authorization successfull.</b></p>"
    "<p>You consented that diary can access your Google calendar.<br/>"
    "Pleasee close this window and return to diary.</p>";
    char http_header[8*1024];

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

//    CURL *curl;
//    CURLcode res;
//
//    curl = curl_easy_init();
//    if (curl) {
//        curl_easy_setops(curl, CURLOPT_URL, "https://accounts.google.com/o/oauth2/auth");
//        res = curl_easy_perform(curl);
//        if (res != CURLE_OK) {
//            fprintf(stderr, "curl_easy_perorm() failed: %s\n", curl_easy_strerror(res));
//        }
//        curl_easy_cleanup(curl);
//    }
}

