#ifndef HTTPSERVE_H
#define HTTPSERVE_H

#include <stdio.h>  // For size_t

// Server configuration constants
#define SERVER_PORT 8080
#define BUFFER_SIZE 16384

// Function prototypes for server operations

// Start the server on a specified port
void start_server(int port);

// Create a TCP socket and configure it
int create_socket(int port);

// Handle incoming connections on the server socket
void handle_connections(int server_sock);

// Process incoming HTTP requests
void process_request(int client_sock);

// Handle GET requests
void handle_get_request(int client_sock, const char* path);

// Handle HEAD requests
void handle_head_request(int client_sock, const char* path);

// Handle POST requests
void handle_post_request(int client_sock, const char* path);

// Send an HTTP response to the client
void send_response(int client_sock, const char *header, const char *content_type, const char *body, int body_length);

// Determine the MIME type based on the file extension
const char* get_mime_type(const char *filename);

#endif // HTTPSERVE_H