#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <sys/wait.h>
#include <errno.h> // Include the necessary header file
#include "httpserve.h"
#define BACKLOG 32 
#define SERVER_ROOT "/home/oscar/Desktop/www/"
#define SERVER_PORT 8080 // Default server port

char http_header[2048];

int main(int argc, char *argv[]) {
    int port = SERVER_PORT;
    if (argc > 1) {
        port = atoi(argv[1]); 
        if (port <= 0) {
            fprintf(stderr, "Invalid port number provided. Using default port %d\n", SERVER_PORT);
            port = SERVER_PORT;  
        }
    }
    start_server(port);
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

    struct sockaddr_in server_addr = {0};
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
    int client_sock;

    while ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addrlen)) >= 0) {
        process_request(client_sock);
    }

    if (client_sock < 0) {
        perror("Error accepting connection");
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
    if (strstr(path, "../") != NULL || strstr(path, "./") != NULL) {
        // Basic protection against directory traversal attacks
        send_response(client_sock, "HTTP/1.1 400 Bad Request", "text/html", "Bad Request: Invalid path.", 0);
        return;
    }

    // Construct the full path to the file
    snprintf(filepath, sizeof(filepath), "%s%s", SERVER_ROOT, path);

    int file_fd = open(filepath, O_RDONLY);
    if (file_fd < 0) {
        // If the file does not exist, return a 404 Not Found error
        perror("Failed to open file");
        send_response(client_sock, "HTTP/1.1 404 Not Found", "text/html", "404 Not Found: The requested resource was not found.", 0);
        return;
    }

    // Determine the size of the file
    struct stat file_stat;
    if (fstat(file_fd, &file_stat) < 0) {
        perror("Failed to get file statistics");
        send_response(client_sock, "HTTP/1.1 500 Internal Server Error", "text/html", "500 Internal Server Error: Unable to retrieve file information.", 0);
        close(file_fd);
        return;
    }

    // Allocate memory and read the file content
    char *file_content = malloc(file_stat.st_size + 1);
    if (read(file_fd, file_content, file_stat.st_size) < 0) {
        perror("Failed to read file");
        send_response(client_sock, "HTTP/1.1 500 Internal Server Error", "text/html", "500 Internal Server Error: Error reading file.", 0);
        free(file_content);
        close(file_fd);
        return;
    }
    file_content[file_stat.st_size] = '\0'; // Null-terminate the content

    
    const char* mime_type = get_mime_type(filepath);

     send_response(client_sock, "HTTP/1.1 200 OK", mime_type, file_content, file_stat.st_size);
    free(file_content);
    close(file_fd);
}





void handle_head_request(int client_sock, const char* path) {
    char filepath[512]; // Buffer to store the full path to the file
    snprintf(filepath, sizeof(filepath), "%s%s", SERVER_ROOT, path); // Construct the full path

    // Prevent directory traversal security issues
    if (strstr(path, "..") != NULL) {
        const char *error_response = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, error_response, strlen(error_response), 0);
        return;
    }

    struct stat file_stat;
    if (stat(filepath, &file_stat) < 0 || S_ISDIR(file_stat.st_mode)) {
        // File does not exist or is a directory
        const char *not_found_response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, not_found_response, strlen(not_found_response), 0);
        return;
    }

    // Prepare and send the headers that a corresponding GET request would return
    char header[256];
snprintf(header, sizeof(header),
         "HTTP/1.1 200 OK\r\n"
         "Content-Type: text/html\r\n"
         "Content-Length: %lld\r\n\r\n", (long long)file_stat.st_size);
send(client_sock, header, strlen(header), 0);
}

void handle_post_request(int client_sock, const char* path) {
    char filepath[512];  // Buffer for constructing the full path to the resource
    snprintf(filepath, sizeof(filepath), "%s%s", SERVER_ROOT, path); // Combine SERVER_ROOT with the path

    // Check if the path corresponds to a CGI script
    if (strstr(filepath, ".cgi") != NULL) {
        int pid = fork();  // Create a new process
        if (pid == 0) {    // Child process
            // Set the environment variable REQUEST_METHOD
            setenv("REQUEST_METHOD", "POST", 1);

            // Duplicate socket on stdout and stderr
            dup2(client_sock, STDOUT_FILENO);
            dup2(client_sock, STDERR_FILENO);

            // Execute the CGI script
            execl(filepath, filepath, NULL);
            perror("Failed to execute CGI script");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {  // Parent process
            int status;
            waitpid(pid, &status, 0);  // Wait for the script to finish
            if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
                const char *error_response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
                send(client_sock, error_response, strlen(error_response), 0);
            }
        } else {  // Fork failed
            const char *error_response = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
            send(client_sock, error_response, strlen(error_response), 0);
        }
    } else {
        const char *not_found_response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, not_found_response, strlen(not_found_response), 0);
    }
}
void send_response(int client_sock, const char *header, const char *content_type, const char *body, int body_length) {
    char response_header[1024]; // Buffer for the HTTP response header

    // Construct the response header
    int header_length = snprintf(response_header, sizeof(response_header),
                                 "%s\r\n"
                                 "Content-Type: %s\r\n"
                                 "Content-Length: %d\r\n"
                                 "\r\n",
                                 header, content_type, body_length);

    // Send the header
    send(client_sock, response_header, header_length, 0);

    // Send the body if it exists and body_length is greater than 0
    if (body && body_length > 0) {
        send(client_sock, body, body_length, 0);
    }
}



const char* get_mime_type(const char *filename) {
    const char *dot = strrchr(filename, '.'); // Find the last occurrence of '.'

    if (!dot || dot == filename) {
        return "application/octet-stream"; // Default MIME type for unknown files
    }

    // Compare the extension and return the MIME type
    if (strcmp(dot, ".html") == 0) return "text/html";
    else if (strcmp(dot, ".css") == 0) return "text/css";
    else if (strcmp(dot, ".js") == 0) return "application/javascript";
    else if (strcmp(dot, ".png") == 0) return "image/png";
    else if (strcmp(dot, ".jpeg") == 0 || strcmp(dot, ".jpg") == 0) return "image/jpeg";
    else if (strcmp(dot, ".gif") == 0) return "image/gif";
    else if (strcmp(dot, ".txt") == 0) return "text/plain";

    return "application/octet-stream"; // Fallback MIME type
}
