#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include "httpserve.h"

#define BACKLOG 32
#define SESSION_ID_LENGTH 16

// In-memory session store (key-value pairs for sessionId -> userData)
typedef struct Session {
    char sessionId[SESSION_ID_LENGTH + 1];
    char userData[256]; // Example: storing user-specific data
} Session;

Session sessionStore[100]; // Simple array-based store
int sessionCount = 0;

// Function prototypes
void logMsg(const char *msg);
void start_server(int port);
int create_socket(int port);
void handle_connections(int server_sock);
void process_request(int client_sock);
void send_response(int client_sock, const char *header, const char *content_type,
                   const char *body, int body_length, const char *cookie);
void handle_get_request(int client_sock, const char *path, char *sessionId);
void generate_session_id(char *sessionId);
char *get_cookie(const char *request, const char *cookie_name);
Session *get_session(const char *sessionId);
Session *create_session();

int main(int argc, char *argv[]) {
    int port = SERVER_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0) {
            fprintf(stderr, "Invalid port. Defaulting to port %d\n", SERVER_PORT);
            port = SERVER_PORT;
        }
    }

    logMsg("Starting server...");
    start_server(port);
    logMsg("Server stopped.");
    return 0;
}

void logMsg(const char *msg) {
    printf("%s\n", msg);
}

void start_server(int port) {
    int server_sock = create_socket(port);
    handle_connections(server_sock);
    close(server_sock);
}

int create_socket(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("Error creating socket");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_adrs = {0};
    server_adrs.sin_family = AF_INET;
    server_adrs.sin_addr.s_addr = INADDR_ANY;
    server_adrs.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&server_adrs, sizeof(server_adrs)) < 0) {
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, BACKLOG) < 0) {
        perror("Error listening on socket");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

void handle_connections(int server_sock) {
    struct sockaddr_in client_addr;
    socklen_t client_addrlen = sizeof(client_addr);
    int client_sock;

    while ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addrlen)) >= 0) {
        logMsg("New connection accepted");
        process_request(client_sock);
    }

    if (client_sock < 0) {
        perror("Error accepting connection");
    }
}

void process_request(int client_sock) {
    char buff[4096];
    int bytes_read = read(client_sock, buff, sizeof(buff) - 1);
    if (bytes_read <= 0) {
        close(client_sock);
        return;
    }

    buff[bytes_read] = '\0';

    // Parse the HTTP request line
    char *method, *path, *protocol, *saveptr;
    method = strtok_r(buff, " ", &saveptr);
    path = strtok_r(NULL, " ", &saveptr);
    protocol = strtok_r(NULL, "\r\n", &saveptr);

    if (!method || !path || !protocol) {
        fprintf(stderr, "Invalid HTTP request\n");
        close(client_sock);
        return;
    }

    // Extract sessionId from the request's cookies
    char *sessionId = get_cookie(buff, "sessionId");

    if (strcmp(method, "GET") == 0) {
        handle_get_request(client_sock, path, sessionId);
    } else {
        const char *response = "HTTP/1.1 405 Method Not Allowed\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, response, strlen(response), 0);
    }

    close(client_sock);
}

void handle_get_request(int client_sock, const char *path, char *sessionId) {
    Session *session = sessionId ? get_session(sessionId) : NULL;

    if (!session) {
        // Create a new session if none exists
        session = create_session();
    }

    // Example response with session tracking
    char body[512];
    snprintf(body, sizeof(body), "<h1>Welcome to the server!</h1><p>Your session ID: %s</p>", session->sessionId);

    // Set the session cookie
    char cookie[256];
    snprintf(cookie, sizeof(cookie), "Set-Cookie: sessionId=%s; Path=/; HttpOnly", session->sessionId);

    send_response(client_sock, "HTTP/1.1 200 OK", "text/html", body, strlen(body), cookie);
}

void send_response(int client_sock, const char *header, const char *content_type,
                   const char *body, int body_length, const char *cookie) {
    char responseHead[2048];
    snprintf(responseHead, sizeof(responseHead),
             "%s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %d\r\n"
             "%s\r\n"
             "\r\n",
             header, content_type, body_length, cookie ? cookie : "");

    send(client_sock, responseHead, strlen(responseHead), 0);

    if (body && body_length > 0) {
        send(client_sock, body, body_length, 0);
    }
}

void generate_session_id(char *sessionId) {
    const char *chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    for (int i = 0; i < SESSION_ID_LENGTH; i++) {
        sessionId[i] = chars[rand() % strlen(chars)];
    }
    sessionId[SESSION_ID_LENGTH] = '\0';
}

char *get_cookie(const char *request, const char *cookie_name) {
    char *cookieHeader = strstr(request, "Cookie: ");
    if (!cookieHeader) return NULL;

    cookieHeader += 8;
    char *cookieStart = strstr(cookieHeader, cookie_name);
    if (!cookieStart) return NULL;

    cookieStart += strlen(cookie_name) + 1;
    char *cookieEnd = strchr(cookieStart, ';');
    if (!cookieEnd) cookieEnd = strchr(cookieStart, '\r');

    size_t cookieLen = cookieEnd ? (size_t)(cookieEnd - cookieStart) : strlen(cookieStart);
    char *cookieValue = malloc(cookieLen + 1);
    strncpy(cookieValue, cookieStart, cookieLen);
    cookieValue[cookieLen] = '\0';
    return cookieValue;
}

Session *get_session(const char *sessionId) {
    for (int i = 0; i < sessionCount; i++) {
        if (strcmp(sessionStore[i].sessionId, sessionId) == 0) {
            return &sessionStore[i];
        }
    }
    return NULL;
}

Session *create_session() {
    if (sessionCount >= 100) return NULL; // Session store full

    Session *newSession = &sessionStore[sessionCount++];
    generate_session_id(newSession->sessionId);
    snprintf(newSession->userData, sizeof(newSession->userData), "User%d", sessionCount);

    return newSession;
}
