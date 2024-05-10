#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h> 
#include <sys/wait.h>
#include <errno.h> 
#include "httpserve.h"
#define BACKLOG 32 


void logMsg(const char *msg); //log function
char httpHead[2048];//buffer for http header

int main(int argc, char *argv[]) {
    int port = SERVER_PORT;//getting port num
    if (argc > 1) {
        port = atoi(argv[1]); //changing port num
        if (port <= 0) {
            fprintf(stderr, "invalid port. Defaulting to set port %d\n", SERVER_PORT);
            port = SERVER_PORT;  
        }
    }
     logMsg("starting server...");//start log msg
    start_server(port);
    logMsg("server stopped.");//end log msg
    return 0;
}
void logMsg(const char *msg) {//log function
    printf("%s\n", msg);
}
void start_server(int port) {//beginnninng of server
    int server_sock = create_socket(port);//call to each function
    handle_connections(server_sock);
    close(server_sock);
}

int create_socket(int port) {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);//intialize socket
    if (sockfd < 0) {
        perror("Error creating socket");//error msg check
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in server_addr = {0};
    server_addr.sin_family = AF_INET;//setting up server address

    server_addr.sin_addr.s_addr = INADDR_ANY;//

    server_addr.sin_port = htons(port);

    if (bind(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {//binding socket
        perror("Bind failed");
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    if (listen(sockfd, BACKLOG) < 0) {//listening on socket
        perror("Error listening on socket");//error msg check
        close(sockfd);
        exit(EXIT_FAILURE);
    }

    return sockfd;
}

void handle_connections(int server_sock) {

    struct sockaddr_in client_addr;//structure for client

    socklen_t client_addrlen = sizeof(client_addr);//settingn size

    int client_sock;

    while ((client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addrlen)) >= 0) {//accepting connection

          logMsg("New connection accepted");//logging
        process_request(client_sock);
    }

    if (client_sock < 0) {

        perror("error accepting");
    }
}


void process_request(int client_sock) {
    char buff[4096]; //buffer for request

    int bytes_read = read(client_sock, buff, sizeof(buff) - 1); // Read the request from the client socket

    if (bytes_read <= 0) {//error check forreaing 
        close(client_sock);
        return;
    }

    buff[bytes_read] = '\0'; //null terminate for string tokenization

    char *method, *path, *protocol, *saveptr;

    method = strtok_r(buff, " ", &saveptr); 

    path = strtok_r(NULL, " ", &saveptr); 

    protocol = strtok_r(NULL, "\r\n", &saveptr); 

    if (!method || !path || !protocol) {//check for valid request
        fprintf(stderr, "Invalid HTTP request line\n");

        close(client_sock);
        return;
    }
    char lgbuff[1024];//buffer for log msg

    snprintf(lgbuff, sizeof(lgbuff), "Received %s request for %s", method, path);
    logMsg(lgbuff);
    
    if (strcmp(method, "GET") == 0) {//checking for method and calling its function
        handle_get_request(client_sock, path);

    } else if (strcmp(method, "HEAD") == 0) {
        handle_head_request(client_sock, path);

    } else if (strcmp(method, "POST") == 0) {
        handle_post_request(client_sock, path);

    } else {
               const char *response = "HTTP/1.1 501 Not a method\r\nContent-Length: 0\r\n\r\n";//just incase of wrong methof
        send(client_sock, response, strlen(response), 0);
    }
    close(client_sock); // Close the client socket after handling the request
}

void handle_get_request(int client_sock, const char* path) {
    char fPath[1024];//giving buffer for file pth

   
    if (strcmp(path, "/") == 0) {//mapping path to correct file path
        strcpy(fPath, "www/index.html");  
    } else {
                snprintf(fPath, sizeof(fPath), "www%s", path);// snprintf to avoid buffer overfloW
    }
     const char* mime_type = get_mime_type(fPath);//getting mime type

    if (mime_type == NULL) {  //error responses 415 invalid media type
        send_response(client_sock, "HTTP/1.1 415 Unsupported Media Type", "text/plain", "415 Unsupported Media Type: file type not supported", 0);
        return;
    }

    struct stat pathStat;

    if (stat(fPath, &pathStat) < 0) {//checking for file
        send_response(client_sock, "HTTP/1.1 404 Not Found", "text/html", "404 Not Found: file not found.", 0);
        return;
    }

    int fileFd = open(fPath, O_RDONLY);//opening file

    if (fileFd < 0) {
        perror("file open failed");
        send_response(client_sock, "HTTP/1.1 404 Not Found", "text/html", "404 Not Found: file not found.", 0);
        return;
    }

    if (fstat(fileFd, &pathStat) < 0) {//checking for file stats
        perror("Failed to get file statistics");
        send_response(client_sock, "HTTP/1.1 500 Internal Server Error", "text/html", "500 Internal Server Error: Couldnt get info.", 0);
        close(fileFd);
        return;
    }

    char *fileCont = malloc(pathStat.st_size + 1);//allocating memory for file content

    if (fileCont == NULL || read(fileFd, fileCont, pathStat.st_size) < 0) {//reading file
        perror("unable to read file");
        send_response(client_sock, "HTTP/1.1 500 Internal Server Error", "text/html", "500 Internal Server Error: unable to read file.", 0);
        free(fileCont);
        close(fileFd);
        return;
    }
    fileCont[pathStat.st_size] = '\0';//null terminate

    
    send_response(client_sock, "HTTP/1.1 200 OK", mime_type, fileCont, pathStat.st_size);//sending response to client

    free(fileCont);//free file content
    close(fileFd);//close file descriptor
}


void handle_head_request(int client_sock, const char* path) {
    char fPath[512]; //setting up buffer for file path

   
    if (strcmp(path, "/") == 0) {//mapping path to correct file path
        strcpy(fPath, "www/index.html");

    } else {
            snprintf(fPath, sizeof(fPath), "www%s", path);
    }

    
    if (strstr(path, "..") != NULL) {//checking for invalid path
        const char *errorMsg = "HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, errorMsg, strlen(errorMsg), 0);
        return;
    }

    struct stat fStat;//structing file stats

    if (stat(fPath, &fStat) < 0 || S_ISDIR(fStat.st_mode)) {//if file not found or its a directory
               const char *notFound = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, notFound, strlen(notFound), 0);
        return;
    }

    
    char header[256];//buffer for header

    snprintf(header, sizeof(header),//printing out header
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"  
             "Content-Length: %lld\r\n\r\n", get_mime_type(fPath), (long long)fStat.st_size);
    send(client_sock, header, strlen(header), 0);//send to client
}

void handle_post_request(int client_sock, const char* path) {// this is an attempt to handle post request. not finished 
    char fPath[512];  //another buff

    if (strcmp(path, "/") == 0) {//mapping once again
        strcpy(fPath, "www/index.html"); 

    } else {
            snprintf(fPath, sizeof(fPath), "www%s", path);
    }

    if (strstr(fPath, ".cgi") != NULL) {//checking for cgi file
        int pid = fork();  

        if (pid == 0) {   //waitpidforking process
            
            setenv("REQUEST_METHOD", "POST", 1);//setting up env variables

            
            dup2(client_sock, STDOUT_FILENO);
            dup2(client_sock, STDERR_FILENO);

            
            execl(fPath, fPath, NULL);//executing cgi script
            perror("didnt execute cgi script");
            exit(EXIT_FAILURE);

        } else if (pid > 0) {  
            int status;

            waitpid(pid, &status, 0); 

            if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {//checking exiting statis
                const char *errorMsg = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
                send(client_sock, errorMsg, strlen(errorMsg), 0);
            }

        } else { 
            const char *errorMsg = "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n";
            send(client_sock, errorMsg, strlen(errorMsg), 0);
        }

    } else {
       
        const char *notFound = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";
        send(client_sock, notFound, strlen(notFound), 0);
    }
}

void send_response(int client_sock, const char *header, const char *content_type, const char *body, int body_length) {
    char responseHead[1024]; //buffer for response header

    
    int headLength = snprintf(responseHead, sizeof(responseHead),//printing out header
                                 "%s\r\n"
                                 "Content-Type: %s\r\n"
                                 "Content-Length: %d\r\n"
                                 "\r\n",
                                 header, content_type, body_length);

    
    send(client_sock, responseHead, headLength, 0);//sending header

    
    if (body && body_length > 0) {//sending body to client and is greater than 0
        send(client_sock, body, body_length, 0);
    }
}

const char* get_mime_type(const char *filename) {
    const char *p = strrchr(filename, '.'); //grabbing file extensio

    if (!p || p == filename) {//making sure its not null
        return NULL;
    }

   
    if (strcmp(p, ".html") == 0) return "text/html";
    else if (strcmp(p, ".css") == 0) return "text/css";
    else if (strcmp(p, ".js") == 0) return "application/javascript";
    else if (strcmp(p, ".png") == 0) return "image/png";
    else if (strcmp(p, ".jpeg") == 0 || strcmp(p, ".jpg") == 0) return "image/jpeg";
    else if (strcmp(p, ".gif") == 0) return "image/gif";
    else if (strcmp(p, ".txt") == 0) return "text/plain";
    else return NULL; //returning null if not there 
}
