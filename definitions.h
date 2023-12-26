#ifndef DEFINITIONS_H
#define DEFINITIONS_H

#include <arpa/inet.h>
#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <libgen.h>
#include <limits.h>
#include <linux/limits.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <pwd.h>
#include <semaphore.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>

// define macros
// PATH_MAX is already an env variable
#define MAX_FILE_NAME_LEN 256
#define MAX_COMMAND_ARGS 10
#define MAX_ACC_PATHS 15
#define MAX_COMMAND_LEN 50000
#define NAMING_SERVER_PORT 8989
#define IP_LEN 16
#define MAX_SS 10 // maximum no. of storage servers
#define MAX_BUFFER_SIZE 6000
#define MAX_ARGS 15
#define CHUNK_SIZE 512
#define MAX_ASSOCIATED_FILES 100

// color coding for printf
#define black "\033[0;30m" 
#define red "\033[0;31m" 
#define green "\033[0;32m" 
#define yellow "\033[0;33m" 
#define blue "\033[0;34m" 
#define pink "\033[0;35m" 
#define teal "\033[0;36m" 
#define white "\033[0;37m" 

// structure to hold the storage server's information
typedef struct StorageServer
{
    char ip[IP_LEN];
    int naming_port;
    int client_port;
    char accessible_paths[MAX_ACC_PATHS][PATH_MAX]; // Maximum of 15 accesible paths displayed
} StorageServer;

// structure to hold the client's information
typedef struct Client
{
    char ip[IP_LEN];
    int port;
} Client;

#endif