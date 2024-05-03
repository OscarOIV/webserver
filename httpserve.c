#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "httpserve.h"
#define BACKLOG 32 
#define SERVER_ROOT "www/" 

int main(int argc, char *argv[]) {// TODO: Parse command line arguments to override default port if necessary
int port = SERVER_PORT;
    if (argc > 1) {
        port = atoi(argv[1]); 
        if (port <= 0) {
            fprintf(stderr, "Invalid port number provided. Using default port %d\n", SERVER_PORT);
            port = SERVER_PORT;  
        }
    }

    start_server(SERVER_PORT);
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
void process_request(int client_sock) {
    char buffer[4096]; // Buffer to store the request
    int bytes_read = read(client_sock, buffer, sizeof(buffer) - 1); // Read the request from the client socket

    if (bytes_read <= 0) {
        perror("Error reading from socket or connection closed");
        close(client_sock);
        return;
    }

    buffer[bytes_read] = '\0'; // Null-terminate the buffer to create a valid string

    // Parse the request line
    char *method, *path, *protocol, *saveptr;
    method = strtok_r(buffer, " ", &saveptr); // Extract the method from the request
    path = strtok_r(NULL, " ", &saveptr); // Extract the path from the request
    protocol = strtok_r(NULL, "\r\n", &saveptr); // Extract the protocol version

    if (!method || !path || !protocol) {
        fprintf(stderr, "Invalid HTTP request line\n");
        close(client_sock);
        return;
    }

    // Dispatch to the appropriate handler based on the method
    if (strcmp(method, "GET") == 0) {
        handle_get_request(client_sock, path);
    } else if (strcmp(method, "HEAD") == 0) {
        handle_head_request(client_sock, path);
    } else if (strcmp(method, "POST") == 0) {
        handle_post_request(client_sock, path);
    } else {
        // If the method is not supported, send a 501 Not Implemented response
        const char *response = "HTTP/1.1 501 Not Implemented\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, response, strlen(response), 0);
    }

    close(client_sock); // Close the client socket after handling the request
}

void handle_get_request(int client_sock, const char* path) {
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s%s", SERVER_ROOT, path);

    // Check for illegal path characters to prevent directory traversal
    if (strstr(path, "../") || strstr(path, "//")) {
        const char *response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, response, strlen(response), 0);
        return;
    }

    FILE *file = fopen(filepath, "rb");
    if (file == NULL) {
        const char *response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, response, strlen(response), 0);
        return;
    }

    // Move to the end of the file to determine the file size
    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    rewind(file);

    // Send the HTTP response headers
    char header[1024];
    snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n", filesize);
    send(client_sock, header, strlen(header), 0);

    // Send the file content
    char buffer[1024];
    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), file)) > 0) {
        send(client_sock, buffer, bytes_read, 0);
    }

    fclose(file);
}


void handle_head_request(int client_sock, const char* path) {
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s%s", SERVER_ROOT, path);

    // Check for illegal path characters to prevent directory traversal
    if (strstr(path, "../") || strstr(path, "//")) {
        const char *response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, response, strlen(response), 0);
        return;
    }

    FILE *file = fopen(filepath, "rb");
    if (file == NULL) {
        const char *response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, response, strlen(response), 0);
        return;
    }

    // Move to the end of the file to determine the file size
    fseek(file, 0, SEEK_END);
    long filesize = ftell(file);
    fclose(file);

    // Send the HTTP response headers
    char header[1024];
    snprintf(header, sizeof(header), "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n", filesize);
    send(client_sock, header, strlen(header), 0);
}

void handle_post_request(int client_sock, const char* path) {
    char filepath[1024];
    snprintf(filepath, sizeof(filepath), "%s%s", SERVER_ROOT, path);

    // Check if the requested path is a CGI script by extension
    if (strstr(path, ".cgi") != NULL) {
        FILE *pipe = popen(filepath, "r"); // Execute the CGI script using popen
        if (pipe == NULL) {
            const char *response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
            send(client_sock, response, strlen(response), 0);
            return;
        }

        // Prepare to capture the output of the script
        char buffer[1024];
        size_t bytes_read;
        const char *header = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\n";
        send(client_sock, header, strlen(header), 0);  // Send header with a simple Content-Type

        // Read the script's output and send it to the client
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), pipe)) > 0) {
            send(client_sock, buffer, bytes_read, 0);
        }

        pclose(pipe);
    } else {
        // If not a CGI script, respond with 404 Not Found
        const char *response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, response, strlen(response), 0);
    }
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
