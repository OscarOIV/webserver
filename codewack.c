#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "httpserve.h"
#define BACKLOG 32 
#define SERVER_ROOT "www/" 

int main(int argc, char *argv[]) {
    int port = SERVER_PORT;  // Assume SERVER_PORT is defined somewhere as the default port
    if (argc > 1) {
        port = atoi(argv[1]);
        if (port <= 0) {
            fprintf(stderr, "Invalid port number provided. Using default port %d\n", SERVER_PORT);
            port = SERVER_PORT;
        }
    }

    start_server(port);  // Use the potentially modified 'port' variable
    return 0;
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

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
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
    
    while (1) {
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addrlen);
        if (client_sock < 0) {
            perror("Error accepting connection");
            continue;
        }

        process_request(client_sock);
    }
}
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>

void process_request(int client_sock) {
    char buffer[4096]; // Buffer to store the request
    int bytes_read = read(client_sock, buffer, sizeof(buffer) - 1); // Read the request from the client socket

    if (bytes_read <= 0) {
        perror("Error reading from socket or connection closed");
        close(client_sock);
        return;
    }

    buffer[bytes_read] = '\0'; // Null-terminate the buffer to ensure it's a valid string

    // Parse the request line
    char *saveptr;
    char *method = strtok_r(buffer, " ", &saveptr); // Extract the method from the request
    char *path = strtok_r(NULL, " ", &saveptr); // Extract the path from the request
    char *protocol = strtok_r(NULL, "\r\n", &saveptr); // Extract the protocol version

    if (!method || !path || !protocol) {
        fprintf(stderr, "Invalid HTTP request line\n");
        close(client_sock);
        return;
    }

    // Simplify the dispatch logic using function pointers array
    void (*request_handler)(int, const char*) = NULL;
    
    if (strcmp(method, "GET") == 0) {
        request_handler = handle_get_request;
    } else if (strcmp(method, "HEAD") == 0) {
        request_handler = handle_head_request;
    } else if (strcmp(method, "POST") == 0) {
        request_handler = handle_post_request;
    }

    if (request_handler) {
        request_handler(client_sock, path);
    } else {
        // If the method is not supported, send a 501 Not Implemented response
        const char *response = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, response, strlen(response), 0);
    }

    close(client_sock); // Ensure the socket is closed after handling the request
}


void handle_get_request(int client_sock, const char* path) {
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s%s", SERVER_ROOT, path);

    if (strstr(path, "../") || strstr(path, "//")) {
        const char *response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, response, strlen(response), 0);
        return;
    }

    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0) {
        const char *response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, response, strlen(response), 0);
        return;
    }

    struct stat file_stat;
    if (fstat(file_fd, &file_stat) < 0) {
        close(file_fd);
        const char *response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, response, strlen(response), 0);
        return;
    }

    const char* mime_type = get_mime_type(filepath);
    char header[1024];
    snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\nContent-Type: %s\r\n\r\n", file_stat.st_size, mime_type);
    send(client_sock, header, strlen(header), 0);

    char buffer[1024];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, buffer, sizeof(buffer))) > 0) {
        send(client_sock, buffer, bytes_read, 0);
    }

    close(file_fd);
}



void handle_head_request(int client_sock, const char* path) {
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s%s", SERVER_ROOT, path);

    if (strstr(path, "../") || strstr(path, "//")) {
        const char *response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, response, strlen(response), 0);
        return;
    }

    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0) {
        const char *response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, response, strlen(response), 0);
        return;
    }

    struct stat file_stat;
    if (fstat(file_fd, &file_stat) < 0) {
        close(file_fd);
        const char *response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, response, strlen(response), 0);
        return;
    }

    char header[1024];
    snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n", file_stat.st_size);
    send(client_sock, header, strlen(header), 0);

    close(file_fd);
}


void handle_post_request(int client_sock, const char* path) {
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s%s", SERVER_ROOT, path);

    // Security enhancement: prevent directory traversal
    if (strstr(path, "../") != NULL || strstr(path, "./") != NULL || strstr(path, ".cgi") == NULL) {
        const char *response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, response, strlen(response), 0);
        return;
    }

    // Execute the CGI script using popen
    FILE *pipe = popen(filepath, "r");
    if (pipe == NULL) {
        perror("Failed to execute script");
        const char *response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, response, strlen(response), 0);
        return;
    }

    char buffer[4096]; // Increased buffer size for potential larger outputs
    size_t bytes_read;
    const char *header = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n";
    send(client_sock, header, strlen(header), 0);  // Assume text/plain for simplicity

    // Stream output from script directly to client
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), pipe)) > 0) {
        send(client_sock, buffer, bytes_read, 0);
    }

    pclose(pipe);
}

void send_response(int client_sock, const char *header, const char *content_type, const char *body, int body_length) {
    // Send the HTTP header first
    send(client_sock, header, strlen(header), 0);

    // Append content type if provided
    if (content_type != NULL) {
        send(client_sock, "Content-Type: ", 14, 0);
        send(client_sock, content_type, strlen(content_type), 0);
        send(client_sock, "\r\n", 2, 0);
    }

    // Append a new line after the header (end of header section)
    send(client_sock, "\r\n", 2, 0);

    // If there is a body to send, send it
    if (body != NULL && body_length > 0) {
        send(client_sock, body, body_length, 0);
    }
}


const char* get_mime_type(const char *filename) {
    const char *dot = strrchr(filename, '.');
    if (dot == NULL) {
        return "text/html"; // Default MIME type
    }

    if (strcmp(dot, ".html") == 0) {
        return "text/html";
    } else if (strcmp(dot, ".css") == 0) {
        return "text/css";
    } else if (strcmp(dot, ".js") == 0) {
        return "application/javascript"; // Correct MIME type for JavaScript
    } else if (strcmp(dot, ".jpg") == 0 || strcmp(dot, ".jpeg") == 0) {
        return "image/jpeg";
    } else if (strcmp(dot, ".png") == 0) {
        return "image/png";
    } else if (strcmp(dot, ".gif") == 0) {
        return "image/gif";
    } else {
        return "text/html"; // Default MIME type
    }
}
