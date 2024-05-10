#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <assert.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX_STRING 4096

// Define a static struct to hold the options
static struct {
    int human;
    int all;
    int format;
    int json;
    int log;
    int inode;
    char* logPath;
    char* path;
} Options = {0, 0, 0, 0, 0, 1, NULL, NULL}; // Initialize all options to default values

void print_console_Output(struct stat* fileInfo, char** argv);
void print_JSON_Output(const char*, const char*, const char*, const char*, const char*, const char*,
                       const char*, const char*, const char*, const char*, const char*);
void help();
int validate_file(struct stat *fileInfo);
char* getNumber(struct stat*);
char* getType(struct stat*);
char* getPermissions(struct stat*);
char* getLinkCount(struct stat*);
char* getUid(struct stat*);
char* getGid(struct stat*);
char* getSize(struct stat*);
char* getAccessTime(struct stat*, int human);
char* getModTime(struct stat*, int human);
char* getStatusChangeTime(struct stat* fileInfo, int human);
void parseargs(int argc, char *argv[]);
int shortArgs(char option);
int longArgs(char* opt);
//void errorOption(char*);
void list_directory(const char *path,struct stat fileInfo, char** argv);
//void errorPath (char* path);


int main(int argc, char *argv[]) {
    struct stat fileInfo = {};
    FILE *logFile = NULL;
    int logFD;

    parseargs(argc, argv);  // Set all flags and paths

    if (Options.log && Options.logPath) {
        logFile = fopen(Options.logPath, "w");
        if (!logFile) {
            perror("Failed to open log file");
            return 1;
        }
        // Get the file descriptor and redirect stdout
        logFD = fileno(logFile);
        if (dup2(logFD, STDOUT_FILENO) == -1) {
            perror("Failed to redirect stdout to log file");
            fclose(logFile);
            return 1;
        }
    }
    // printf("[\n");
    // Existing code logic for processing files
    if (Options.all) {
        printf("[\n");
        const char *path = Options.path ? Options.path : ".";
        list_directory(path, fileInfo, argv);
        printf("]\n");
    } else if (Options.path && validate_file(&fileInfo) == 0) {
        if (Options.json) {
            print_JSON_Output(Options.path, getNumber(&fileInfo), getType(&fileInfo), getPermissions(&fileInfo), getLinkCount(&fileInfo), getUid(&fileInfo), getGid(&fileInfo), getSize(&fileInfo), getAccessTime(&fileInfo, Options.human), getModTime(&fileInfo, Options.human), getStatusChangeTime(&fileInfo, Options.human));
            // printf("]\n");
        } else {
            print_console_Output(&fileInfo, argv);
        }
    }

    if (logFile) {
        fclose(logFile);  // Close the log file properly
    }

    return 0;
}

int validate_file(struct stat *fileInfo) {
    if (Options.path != NULL && stat(Options.path, fileInfo) != 0) {
        fprintf(stderr, "Error getting file info for %s: %s\n", Options.path, strerror(errno));
        return 1;  // Return 1 to indicate an error
    }
    return 0;  // Return 0 to indicate success
}

//loops to check the files in the directory
void list_directory(const char *path, struct stat fileInfo, char** argv) {
    DIR *dir;
    struct dirent *entry;
    struct stat fileStat;  // Use a local stat structure to ensure it's fresh each loop.

    char fullPath[MAX_STRING];  // Buffer to hold the full path of the files

    if (!(dir = opendir(path))) {
        perror("Failed to open directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Construct the full path of the file
        snprintf(fullPath, sizeof(fullPath), "%s/%s", path, entry->d_name);

        // Check if we can get file stats; if not, continue to the next file
        if (stat(fullPath, &fileStat) != 0) {
            fprintf(stderr, "Failed to get stats for %s: %s\n", fullPath, strerror(errno));
            continue;
        }

        if (Options.json) {
            print_JSON_Output(fullPath, getNumber(&fileStat), getType(&fileStat),
                              getPermissions(&fileStat), getLinkCount(&fileStat), getUid(&fileStat),
                              getGid(&fileStat), getSize(&fileStat), getAccessTime(&fileStat, Options.human),
                              getModTime(&fileStat, Options.human), getStatusChangeTime(&fileStat, Options.human));
            // printf("]\n");
        } else {
            print_console_Output(&fileStat, argv);
        }
    }
    closedir(dir);
}

//do redirection, before parsing and printing so log stuff
void parseargs(int argc, char **argv) {
    // validate there are enough args no put help 
    if (argc <= 1) {
        help();
        exit(EXIT_FAILURE);
    }

    for (int i = 1; i < argc; i++) {
        // command line arg is an option
        if (argv[i][0] == '-') {
            if (strcmp(argv[i], "--help") == 0) {
                    help();
                    exit(EXIT_SUCCESS);
            }
            // command line arg uses --someword format
            if (argv[i][1] == '-') {
                if (!longArgs(argv[i])) { // Check for long arguments like "--help"
                    fprintf(stderr, "Invalid option %s\n", argv[i]);
                    exit(EXIT_FAILURE);
                }
                // Handle specific long options that may require additional parameters
                if (strcmp(argv[i], "--log") == 0 && i + 1 < argc) {
                    Options.logPath = argv[++i]; // Increment to skip next argument
                }
            } else {
                // command line arg uses short -X format
                for (int j = 1; argv[i][j] != '\0'; j++) {
                    if (!shortArgs(argv[i][j])) { // Check for short arguments like "-h"
                        fprintf(stderr, "Invalid option -%c\n", argv[i][j]);
                        exit(EXIT_FAILURE);
                    }
                    // Handle specific short options that may require additional parameters
                    if (argv[i][j] == 'l' && i + 1 < argc) {
                        Options.logPath = argv[++i];
                        break; // Break to avoid processing further characters
                    }
                }
            }
        } else {
            // This part is used to handle format types or paths directly
            if (strcmp(argv[i], "json") == 0) {
                Options.json = 1;
            } else if (strcmp(argv[i], "text") == 0) {
                Options.format = 1;
            } else {
                // Assume it's a path if no specific format was mentioned
                if (!Options.path) {
                    Options.path = argv[i];
                } else {
                    //fprintf(stderr, "Multiple paths provided. Please specify only one path.\n"); // this is wrong allow multiple paths 
                    exit(EXIT_FAILURE);
                }
            }
        }
    }

    // Check if path or 'all' option is not specified
    if (!Options.all && !Options.path) {
        fprintf(stderr, "Error: No path provided or 'all' not specified.\n");
        help();
        exit(EXIT_FAILURE);
    }
}


int shortArgs(char option) {
    switch (option) {
        case '?':
            help();
            return 0;
        case 'i':
            Options.inode = 1;
            return 1;
        case 'a':
            Options.all = 1;
            return 1;
        case 'h':
            Options.human = 1;
            return 1;
        case 'f':
            Options.format = 1;
            return 1;
        case 'l':
            Options.log = 1;
            return 1;
        default:
            fprintf(stderr, "Error: Unknown option '-%c'.\n", option);
            exit(EXIT_FAILURE);
    }
    return 0; // Default case if no match found
}

int longArgs(char* opt) {
    if (strcmp(opt, "--help") == 0) {
        help();
        return 1;
    }
    if (strcmp(opt, "--inode") == 0) {
        Options.inode = 1;
        return 1;
    }
    if (strcmp(opt, "--all") == 0) {
        Options.all = 1;
        return 1;
    }
    if (strcmp(opt, "--human") == 0) {
        Options.human = 1;
        return 1;
    }
    if (strcmp(opt, "--format") == 0) {
        Options.format = 1;
        return 1;
    }
    if (strcmp(opt, "--log") == 0) {
        Options.log = 1;
        return 1;
    }
    return 0; // Return 0 if no valid option was matched
}
 
void print_console_Output(struct stat* fileInfo, char** argv){
    printf("\nInfo for: %s:\n", argv[1]);
    printf("File Inode: %llu\n", fileInfo->st_ino);
    printf("File Type: ");
    if (S_ISREG(fileInfo->st_mode))
        printf("regular file\n");
    else if (S_ISDIR(fileInfo->st_mode))
        printf("directory\n");
    else if (S_ISCHR(fileInfo->st_mode))
        printf("character device\n");
    else if (S_ISBLK(fileInfo->st_mode))
        printf("block device\n");
    else if (S_ISFIFO(fileInfo->st_mode))
        printf("FIFO (named pipe)\n");
    else if (S_ISLNK(fileInfo->st_mode))
        printf("symbolic link\n");
    else if (S_ISSOCK(fileInfo->st_mode))
        printf("socket\n");
    else
        printf("unknown?\n");

    //get it human readable 
    char* sizeStr = getSize(fileInfo);  
    char* accessTime = getAccessTime(fileInfo, Options.human);
    char* modTime = getModTime(fileInfo, Options.human);
    char* statusChangeTime = getStatusChangeTime(fileInfo, Options.human);

    printf("Number of Hard Links: %hu\n", fileInfo->st_nlink);
    printf("File Size: %s\n", sizeStr);  // Use formatted size
    printf("Last Access Time: %s\n", accessTime);
    printf("Last Modification Time: %s\n", modTime);
    printf("Last Status Change Time: %s\n\n", statusChangeTime);

    // printf("\nChecking to see if the Options Values are changing....\n\n");
    // printf("Options:\n");
    // printf("  human: %d\n", Options.human);
    // printf("  all: %d\n", Options.all);
    // printf("  format: %d\n", Options.format);
    // printf("  json: %d\n", Options.json);
    // printf("  log: %d\n", Options.log);
    // printf("  inode: %d\n", Options.inode);
    // printf("  logPath: %s\n", Options.logPath ? Options.logPath : "NULL");
    // printf("  path: %s\n", Options.path ? Options.path : "NULL");

    free(accessTime);
    free(modTime);
    free(statusChangeTime);
}

void print_JSON_Output(const char* path, const char* number, const char* type,
                         const char* permissions, const char* linkCount,
                         const char* uid, const char* gid, const char* size,
                         const char* accessTime, const char* modTime,
                         const char* statusChangeTime){
    static char opt[MAX_STRING]; // Fixed-size character array
    int length = snprintf(opt, MAX_STRING,
       "  {\n"
    "    \"filepath\": \"%s\",\n"
    "    \"inode\": {\n"
    "      \"number\": %s,\n"
    "      \"type\": \"%s\",\n"
    "      \"permissions\": \"%s\",\n"
    "      \"linkCount\": %s,\n"
    "      \"uid\": %s,\n"
    "      \"gid\": %s,\n"
    "      \"size\": %s,\n"
    "      \"accessTime\": %s,\n"
    "      \"modificationTime\": %s,\n"
    "      \"statusChangeTime\": %s\n"
    "    }\n"
    "  },\n",
        path, number, type, permissions, linkCount, uid,
        gid, size, accessTime, modTime, statusChangeTime);
    if (length >= MAX_STRING) {
        // Handle overflow error here
        // This implementation does not support handling overflow
    }
    printf("%s", opt);
    // printf("\nOptions:\n");
    // printf("  human: %d\n", Options.human);
    // printf("  all: %d\n", Options.all);
    // printf("  format: %d\n", Options.format);
    // printf("  json: %d\n", Options.json);
    // printf("  log: %d\n", Options.log);
    // printf("  inode: %d\n", Options.inode);
    // printf("  logPath: %s\n", Options.logPath ? Options.logPath : "NULL");
    // printf("  path: %s\n", Options.path ? Options.path : "NULL"); //add this to file path 
}

char* getNumber(struct stat *fileInfo) {
    static char buffer[20];
    snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)fileInfo->st_ino);
    return buffer;
}

char* getType(struct stat *fileInfo) {
    static char type[20]; // Assuming the type won't exceed 20 characters
    const char* types[] = {
        "regular file",
        "directory",
        "character device",
        "block device",
        "FIFO",
        "symbolic link",
        "socket"
    };
    mode_t mode = fileInfo->st_mode;
    int i = 0;
    while (i < 7) {
        if (S_ISREG(mode)) {
            strcpy(type, types[0]);
            break;
        } else if (S_ISDIR(mode)) {
            strcpy(type, types[1]);
            break;
        } else if (S_ISCHR(mode)) {
            strcpy(type, types[2]);
            break;
        } else if (S_ISBLK(mode)) {
            strcpy(type, types[3]);
            break;
        } else if (S_ISFIFO(mode)) {
            strcpy(type, types[4]);
            break;
        } else if (S_ISLNK(mode)) {
            strcpy(type, types[5]);
            break;
        } else if (S_ISSOCK(mode)) {
            strcpy(type, types[6]);
            break;
        } else {
            strcpy(type, "unknown");
            break;
        }
        i++;
    }
    return type;
}

char* getPermissions(struct stat *fileInfo) {
    static char str1[11];
    int i = 0;
    str1[i++] = (S_ISDIR(fileInfo->st_mode)) ? 'd' : '-';
    str1[i++] = (fileInfo->st_mode & S_IRUSR) ? 'r' : '-';
    str1[i++] = (fileInfo->st_mode & S_IWUSR) ? 'w' : '-';
    str1[i++] = (fileInfo->st_mode & S_IXUSR) ? 'x' : '-';
    str1[i++] = (fileInfo->st_mode & S_IRGRP) ? 'r' : '-';
    str1[i++] = (fileInfo->st_mode & S_IWGRP) ? 'w' : '-';
    str1[i++] = (fileInfo->st_mode & S_IXGRP) ? 'x' : '-';
    str1[i++] = (fileInfo->st_mode & S_IROTH) ? 'r' : '-';
    str1[i++] = (fileInfo->st_mode & S_IWOTH) ? 'w' : '-';
    str1[i++] = (fileInfo->st_mode & S_IXOTH) ? 'x' : '-';
    str1[i++] = '\0';
    return str1;
}

char* getLinkCount(struct stat *fileInfo) {
    static char buffer[20];
    snprintf(buffer, sizeof(buffer), "%lu", (unsigned long)fileInfo->st_nlink);
    return buffer;
}

char* getUid(struct stat *fileInfo) {
    static char buffer[20];
    snprintf(buffer, sizeof(buffer), "%u", fileInfo->st_uid);
    return buffer;
}

char* getGid(struct stat *fileInfo) {
    static char buffer[20];
    snprintf(buffer, sizeof(buffer), "%u", fileInfo->st_gid);
    return buffer;
}

char* getSize(struct stat *fileInfo) {
    //printf("this is running i think\n");
    static char buf[64]; // Static buffer for the size string
    long long size = fileInfo->st_size;
    if (Options.human) {
        //printf("working!\n");
        static const char *SIZES[] = { "B", "K", "M", "G", "T", "P", "E" }; // Include all units up to exabytes
        int div = 0;

        while (size >= 1024 && div < (sizeof SIZES / sizeof *SIZES) - 1) {
            size /= 1024;
            div++;
        }

        if (div == 0) {  // Bytes do not need a decimal
            snprintf(buf, sizeof(buf), "%lld%s", size, SIZES[div]);
        } else {  // Other units get one decimal for precision
            snprintf(buf, sizeof(buf), "%.1f%s", (double)size, SIZES[div]);
        }
    } else {
        snprintf(buf, sizeof(buf), "%lld", size);
    }

    return buf; // Return the buffer containing the formatted size
}

char* getAccessTime(struct stat* fileInfo, int human) {
    if (human) {
        struct tm *info;
        // Convert time_t to broken-down time representation
        info = localtime(&fileInfo->st_atime);
        // Calculate the required buffer size for the formatted string
        int n = snprintf(NULL, 0, "\"%04d-%02d-%02d %02d:%02d:%02d\"",
                         info->tm_year + 1900, info->tm_mon + 1,
                         info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);
        assert(n > 0); // Ensure the calculation is successful
        // Allocate memory for the formatted string
        char* formatted_time = malloc(n + 1);
        if (!formatted_time) {
            return NULL; // Return NULL if memory allocation fails
        }
        // Format the date and time into the allocated buffer
        snprintf(formatted_time, n + 1, "\"%04d-%02d-%02d %02d:%02d:%02d\"",
                 info->tm_year + 1900, info->tm_mon + 1,
                 info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);
        return formatted_time;
    } else {
        time_t seconds = fileInfo->st_atime;
        // Calculate the buffer size needed for the timestamp
        int n = snprintf(NULL, 0, "%ld", seconds);
        assert(n > 0); // Ensure the calculation is successful
        // Allocate memory for the timestamp string
        char* formatted_time = malloc(n + 1);
        if (!formatted_time) {
            return NULL; // Return NULL if memory allocation fails
        }
        // Format the timestamp into the buffer
        snprintf(formatted_time, n + 1, "%ld", seconds);
        return formatted_time;
    }
}

char* getModTime(struct stat* fileInfo, int human) {
    if (human) {
        struct tm *info;
        // Convert time_t to broken-down time representation
        info = localtime(&fileInfo->st_mtime); // Use st_mtime for modification time
        // Calculate the required buffer size for the formatted string
        int n = snprintf(NULL, 0, "\"%04d-%02d-%02d %02d:%02d:%02d\"",
                         info->tm_year + 1900, info->tm_mon + 1,
                         info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);
        assert(n > 0); // Ensure the calculation is successful
        // Allocate memory for the formatted string
        char* formatted_time = malloc(n + 1);
        if (!formatted_time) {
            return NULL; // Return NULL if memory allocation fails
        }
        // Format the date and time into the allocated buffer
        snprintf(formatted_time, n + 1, "\"%04d-%02d-%02d %02d:%02d:%02d\"",
                 info->tm_year + 1900, info->tm_mon + 1,
                 info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);
        return formatted_time;
    } else {
        time_t seconds = fileInfo->st_mtime; // Use st_mtime for modification time
        // Calculate the buffer size needed for the timestamp
        int n = snprintf(NULL, 0, "%ld", seconds);
        assert(n > 0); // Ensure the calculation is successful
        // Allocate memory for the timestamp string
        char* formatted_time = malloc(n + 1);
        if (!formatted_time) {
            return NULL; // Return NULL if memory allocation fails
        }
        // Format the timestamp into the buffer
        snprintf(formatted_time, n + 1, "%ld", seconds);
        return formatted_time;
    }
}

char* getStatusChangeTime(struct stat* fileInfo, int human) {
    if (human) {
        struct tm *info;
        // Convert time_t to broken-down time representation
        info = localtime(&fileInfo->st_ctime); // Use st_ctime for status change time
        // Calculate the required buffer size for the formatted string
        int n = snprintf(NULL, 0, "\"%04d-%02d-%02d %02d:%02d:%02d\"",
                         info->tm_year + 1900, info->tm_mon + 1,
                         info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);
        assert(n > 0); // Ensure the calculation is successful
        // Allocate memory for the formatted string
        char* formatted_time = malloc(n + 1);
        if (!formatted_time) {
            return NULL; // Return NULL if memory allocation fails
        }
        // Format the date and time into the allocated buffer
        snprintf(formatted_time, n + 1, "\"%04d-%02d-%02d %02d:%02d:%02d\"",
                 info->tm_year + 1900, info->tm_mon + 1,
                 info->tm_mday, info->tm_hour, info->tm_min, info->tm_sec);
        return formatted_time;
    } else {
        time_t seconds = fileInfo->st_ctime; // Use st_ctime for status change time
        // Calculate the buffer size needed for the timestamp
        int n = snprintf(NULL, 0, "%ld", seconds);
        assert(n > 0); // Ensure the calculation is successful
        // Allocate memory for the timestamp string
        char* formatted_time = malloc(n + 1);
        if (!formatted_time) {
            return NULL; // Return NULL if memory allocation fails
        }
        // Format the timestamp into the buffer
        snprintf(formatted_time, n + 1, "%ld", seconds);
        return formatted_time;
    }
}

void help(){
  // prints the command line options 
   printf("Usage: inspect [OPTION]... [PATH]...\n");
  printf("\nDisplay information for a file.\n\n");
  printf("Command-Line Options\n");
  printf("  -?, --help:                 Display help information about the tool and its options.\n");
  printf("   Example: inspect -?\n");
  printf("  -i, --inode <file_path>:    Display detailed inode information for the specified file.\n");
  printf("   Example: inspect -i /path/to/file\n");
  printf("   Note that this flag is optional and the default behavior is\n");
  printf("   identical if the flag is omitted.\n");
  printf("  -a, --all [directory_path]: Display inode information for all files within the specified directory. If nopaths is provided, default to the current directory.\n");
  printf("   Example: inspect -a /path/to/directory -r\n");
  printf("  -h, --human:                Output all sizes in kilobytes (K), megabytes (M), or gigabytes(G) and all dates in a human-readable form.\n");
  printf("   Example: inspect -i /path/to/file -h\n");
  printf("  -f, --format [text|json]:   Specify the output format. If not specified, default to plain text.\n");
  printf("   Example: inspect -i /path/to/file -f json\n");
  printf("  -l, --log <log_file>:       Log operations to a specified file.\n");
  printf("   Example: inspect -i /path/to/file -l /path/to/logfile\n\n");
}

