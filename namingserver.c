// include definitions
#include "definitions.h"
// includes any required utility functions
#include "hashmap/map_operations.h"
#include "LRU/lru.h"

char *ip = "127.0.0.1";
int storage_server_count = 0;
struct StorageServer StorageServersInfo[MAX_SS];
char naming_server_gets[MAX_BUFFER_SIZE];
char naming_server_sends[MAX_BUFFER_SIZE];
int used_ports[MAX_SS * 2 + 1];

hashmap *accessible_paths = NULL;
hashmap *LRU_Map = NULL;
LRU *lru = NULL;   

int processCommand(char *COMMAND, char ARGS[][PATH_MAX], int *no_args)
{
    int len = strlen(COMMAND);

    for (int i = 0; i < len; i++)
    {
        if (COMMAND[i] == '\t' || COMMAND[i] == '\n')
        {
            COMMAND[i] = ' ';
        }
    }

    int idx = 0;
    char *token = strtok(COMMAND, " ");
    strcpy(ARGS[idx], token);
    while (1)
    {
        idx++;
        token = strtok(NULL, " ");
        if (token == NULL)
            break;
        strcpy(ARGS[idx], token);
    }

    *no_args = idx;
    // printf(green "[+]Processed %d arguments\n" white, *no_args);

    return 0;
}

int connectToStorageServer(char storage_server_ip[], int storage_server_port, char ARGS[][PATH_MAX], int no_args)
{
    // Create a socket for communication
    int storageSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (storageSocket < 0)
    {
        perror(red "[-] Socket creation" white);
        exit(EXIT_FAILURE);
    }

    // Set up the server address
    struct sockaddr_in storageServerAddr;
    bzero(&storageServerAddr, sizeof(storageServerAddr));
    storageServerAddr.sin_family = AF_INET;
    storageServerAddr.sin_port = htons(storage_server_port);
    storageServerAddr.sin_addr.s_addr = inet_addr(storage_server_ip);

    if (inet_pton(AF_INET, storage_server_ip, &storageServerAddr.sin_addr) <= 0)
    {
        perror(red "[-] Address conversion" white);
        exit(EXIT_FAILURE);
    }

    // Connect to the storage server
    if (connect(storageSocket, (struct sockaddr *)&storageServerAddr, sizeof(storageServerAddr)) < 0)
    {
        perror(red "[-] Connection to storage server" white);
        exit(EXIT_FAILURE);
    }
    printf(green "[+]Connected to storage server\n" white);
    printf(yellow "[+]Storage server ip: %s\n" white, storage_server_ip);
    printf(yellow "[+]Storage server port: %d\n" white, storage_server_port);

    // send the number of args to the storage server first
    if (send(storageSocket, &no_args, sizeof(no_args), 0) < 0)
    {
        perror(red "[-] Send" white);
        exit(EXIT_FAILURE);
    }

    char naming_server_gets[MAX_BUFFER_SIZE];
    char naming_server_sends[MAX_BUFFER_SIZE];

    // send the command to the storage server
    for (int i = 0; i < no_args; i++)
    {
        printf(teal "[+]Sending: %s\n" white, ARGS[i]);

        // send the arguments one by one
        if (send(storageSocket, ARGS[i], sizeof(ARGS[i]), 0) < 0)
        {
            perror(red "[-] Send" white);
            exit(EXIT_FAILURE);
        }

        // receive acknowledgement from storage server
        bzero(naming_server_gets, MAX_BUFFER_SIZE);
        if (recv(storageSocket, naming_server_gets, sizeof(naming_server_gets), 0) < 0)
        {
            perror(red "[-] Recv" white);
            exit(EXIT_FAILURE);
        }

        // check if the acknowledgement is an ACK
        if (strcasecmp(naming_server_gets, "ACK") != 0)
        {
            printf(red "[-]Error in sending command to storage server -> 1\n" white);
            printf(yellow "[-]Storage server ip: %s\n" white, storage_server_ip);
            printf(yellow "[-]Storage server port: %d\n" white, storage_server_port);
            exit(EXIT_FAILURE);
        }
    }

    // send final done packet to storage server
    bzero(naming_server_sends, MAX_BUFFER_SIZE);
    strcpy(naming_server_sends, "DONE");
    if (send(storageSocket, naming_server_sends, sizeof(naming_server_sends), 0) < 0)
    {
        perror(red "[-] Send" white);
        exit(EXIT_FAILURE);
    }

    // return the socket number
    return storageSocket;
}

// Function to send a command to a storage server and accordingly send the result to the client
// the 1 - if the operation is a success and 0 - if operation is a failure
int getResultsFromSS(char ip[], int port, char ARGS[][PATH_MAX], int no_args)
{
    // connect to storage server and get the socket number
    int storageSocket = connectToStorageServer(ip, port, ARGS, no_args);

    // receive final result of the entire operation from the storage server, an ACK is received if the operation is performed properly
    bzero(naming_server_gets, MAX_BUFFER_SIZE);
    if (recv(storageSocket, naming_server_gets, sizeof(naming_server_gets), 0) < 0)
    {
        perror(red "[-] Recv" white);
        exit(EXIT_FAILURE);
    }

    int result = 0;

    if (strcasecmp(naming_server_gets, "ACK") == 0)
    {
        result = 1;
    }

    // Close the socket
    printf(teal "[+]Closed socket for storage server connection\n" white);
    printf(yellow "[+]Storage server ip: %s\n" white, ip);
    printf(yellow "[+]Storage server port: %d\n" white, port);
    close(storageSocket);

    return result;
}

int facilitateCopy(int storageSocket1, int storageSocket2)
{
    char buffer[CHUNK_SIZE + 1];

    // get the file name from the storage server 1
    bzero(buffer, CHUNK_SIZE + 1);
    if (recv(storageSocket1, buffer, sizeof(buffer), 0) < 0)
    {
        perror(red "[-] Recv" white);
        exit(EXIT_FAILURE);
    }
    printf(teal "[+]File name received from storage server 1: %s\n" white, buffer);
    
    // send the file name to the storage server 2
    if (send(storageSocket2, buffer, sizeof(buffer), 0) < 0)
    {
        perror(red "[-] Send" white);
        exit(EXIT_FAILURE);
    }
    printf(teal "[+]File name sent to storage server 2: %s\n" white, buffer);

    // send the file data from storage server 1 to storage server 2 in chunks
    bzero(buffer, CHUNK_SIZE + 1);
    while (recv(storageSocket1, buffer, sizeof(buffer), 0) > 0)
    {
        if (strcasecmp(buffer, "STOP") == 0)
        {
            printf(teal "[+]STOP\n" white);
            break;
        }

        // printf(teal "[+]Sending file data from storage server 1 to storage server 2: %s\n" white, buffer);
        if (send(storageSocket2, buffer, sizeof(buffer), 0) < 0)
        {
            perror(red "[-] Send" white);
            exit(EXIT_FAILURE);
        }

        bzero(buffer, CHUNK_SIZE + 1);
    }

    // sleep(1);

    // send final stop packet to storage server 2
    if (send(storageSocket2, "STOP", sizeof("STOP"), 0) < 0)
    {
        perror(red "[-] Send" white);
        exit(EXIT_FAILURE);
    } 

    // get packet from ss2 about if it was a file or dir
    int zip = 0;
    if (recv(storageSocket2, &zip, sizeof(zip), 0) < 0)
    {
        perror(red "[-] Recv" white);
        exit(EXIT_FAILURE);
    }

    // get final acknowledgement from storage server 1
    char final_ack1[MAX_BUFFER_SIZE];
    bzero(final_ack1, MAX_BUFFER_SIZE);
    if (recv(storageSocket1, final_ack1, sizeof(final_ack1), 0) < 0)
    {
        perror(red "[-] Recv" white);
        exit(EXIT_FAILURE);
    }
    printf(teal "[+]Final acknowledgement from storage server 1: %s\n" white, final_ack1);

    // get final acknowledgement from storage server 2
    char final_ack2[MAX_BUFFER_SIZE];
    bzero(final_ack2, MAX_BUFFER_SIZE);
    if (recv(storageSocket2, final_ack2, sizeof(final_ack2), 0) < 0)
    {
        perror(red "[-] Recv" white);
        exit(EXIT_FAILURE);
    }
    printf(teal "[+]Final acknowledgement from storage server 2: %s\n" white, final_ack2);

    int result = 0;

    if (strcasecmp(final_ack1, "ACK") == 0 && strcasecmp(final_ack2, "ACK") == 0)
    {
        if (zip == 0)
        {
            // file
            result = 1;
        }
        else
        {
            // directory
            result = 2;
        }
    }

    // close the sockets
    // print ip and port
    printf(teal "[+]Closed socket for storage server 1 connection\n" white);
    close(storageSocket1);
    printf(teal "[+]Closed socket for storage server 2 connection\n" white);
    close(storageSocket2);

    return result;
}

// function to handle client connections
void *client_handler(void *pclient)
{
    int clientSocket = *(int *)pclient;
    free(pclient);
    int client_id = clientSocket;
    printf(teal "[+]New client id: %d\n" white, client_id);

    char client_gets[MAX_BUFFER_SIZE];
    char client_sends[MAX_BUFFER_SIZE];

    // initialize the ARGS array
    char ARGS[MAX_ARGS][PATH_MAX];

    int no_args;

    printf(yellow "LRU says %d for %s\n" white,get_LRU(lru,"/operations"),"/operations");

    while (1)
    {
        // receive the response
        bzero(client_sends, MAX_BUFFER_SIZE);
        if (recv(clientSocket, client_sends, sizeof(client_sends), 0) < 0)
        {
            perror(red "[-] Recv" white);
            exit(EXIT_FAILURE);
        }

        // print what the client has sent
        printf(teal "[+]Client %d sent: %s" white, client_id, client_sends);
        // get client ip and port from the socket
        struct sockaddr_in clientAddr;
        socklen_t addr_size = sizeof(clientAddr);
        getpeername(clientSocket, (struct sockaddr *)&clientAddr, &addr_size);
        printf(yellow "[+]Client ip: %s\n" white, inet_ntoa(clientAddr.sin_addr));
        printf(yellow "[+]Client port: %d\n" white, ntohs(clientAddr.sin_port));

        // NOTE: added processCommand function here
        // COMMAND: client_sends
        char COMMAND[MAX_COMMAND_LEN];
        strcpy(COMMAND, client_sends);
        processCommand(COMMAND, ARGS, &no_args);

        int command_type = 0;

        if (!strcasecmp(ARGS[0], "CREATE") || !strcasecmp(ARGS[0], "DELETE"))
        {   
            command_type = 1;
            // send the command type to the cilent
            if (send(clientSocket, &command_type, sizeof(command_type), 0) < 0)
            {
                perror(red "[-] Send" white);
                exit(EXIT_FAILURE);
            }

            // go through all the storage servers
            // compare the ARGS[1] with the accessible paths of each storage server
            // if it matches, then send the command to that storage server
            // if it doesn  't match, then send an error message to the client
            if (ARGS[1] == NULL)
            {
                // send an error message to the client
                strcpy(client_gets, "Invalid command. Command path is not mentioned. -> 103");
                if (send(clientSocket, client_gets, sizeof(client_gets), 0) < 0)
                {
                    perror(red "[-] Send" white);
                    exit(EXIT_FAILURE);
                }
            }

            // check if the path is valid
            bool valid_path = false;
            int storage_server_num = get_value(accessible_paths, ARGS[1]);
            if (storage_server_num != -1)
            {
                valid_path = true;
                printf(teal "[+]Path is present in storage server %d\n" white, storage_server_num + 1);
                printf(teal "[+]Sending command to the storage server\n" white);
                // for (int i = 0; i < no_args; i++)
                // {
                //     printf("command: %s\n" white, ARGS[i]);
                // }
                int result = getResultsFromSS(StorageServersInfo[storage_server_num].ip, StorageServersInfo[storage_server_num].naming_port, ARGS, no_args);
                
                // send acknowledgement to the client
                bzero(client_gets, MAX_BUFFER_SIZE);
                if (result == 1)
                {
                    strcpy(client_gets, "ACK");
                    char new_path[PATH_MAX*2];
                    // make modifications to the hash map accordingly as well 
                    if (strcasecmp(ARGS[0], "CREATE") == 0)
                    {   
                        snprintf(new_path, sizeof(new_path), "%s/%s", ARGS[1], ARGS[2]);
                        printf(teal "[+]Adding to list of accessible paths %s\n" white, new_path);
                        set_value(accessible_paths, new_path, storage_server_num);
                        printf(yellow "[+]Accessible paths\n");
                        print_map(accessible_paths);
                        printf(white);
                    }
                    else if(strcasecmp(ARGS[0], "DELETE") == 0)
                    {   
                        snprintf(new_path, sizeof(new_path), "%s", ARGS[1]);
                        printf(teal "[+]Deleting from the list of accessible paths %s\n" white, new_path);
                        remove_entry(accessible_paths, new_path);
                        printf(yellow "[+]Accessible paths\n");
                        print_map(accessible_paths);
                        printf(white);
                    }

                    printf(green "[+]Operation was a success\n" white);
                }
                else
                {
                    strcpy(client_gets, "FAIL");

                    printf(red "[-]Operation was a failure -> 104\n" white);
                }
                
                if (send(clientSocket, client_gets, sizeof(client_gets), 0) < 0)
                {
                    perror(red "[-] Send" white);
                    exit(EXIT_FAILURE);
                }
            }

            // if the path is invalid
            if (!valid_path)
            {   
                printf(red "[-]Path not found -> 105\n" white);
                sprintf(client_gets, "Path isn't present in any of our storage servers. COMMAND: %s <PATH> <FILENAME>", ARGS[0]);
                if (send(clientSocket, client_gets, sizeof(client_gets), 0) < 0)
                {
                    perror(red "[-] Send" white);
                    exit(EXIT_FAILURE);
                }
            }
        }
        else if (strcasecmp(ARGS[0], "READ") == 0 || strcasecmp(ARGS[0], "WRITE") == 0)
        {
            if (strcasecmp(ARGS[0], "READ") == 0)
            {
                command_type = 2;
            }
            else if (strcasecmp(ARGS[0], "WRITE") == 0)
            {
                command_type = 3;
            }

            // send the command type to the cilent
            if (send(clientSocket, &command_type, sizeof(command_type), 0) < 0)
            {
                perror(red "[-] Send" white);
                exit(EXIT_FAILURE);
            }

            if (ARGS[1] == NULL)
            {
                // send an error message to the client
                sprintf(client_gets, "Invalid command. COMMAND: %s <PATH> -> 103", ARGS[0]);
                if (send(clientSocket, client_gets, sizeof(client_gets), 0) < 0)
                {
                    perror(red "[-] Send" white);
                    exit(EXIT_FAILURE);
                }
            }

            // check if the path is valid
            bool valid_path = false;
            int storage_server_num = get_value(accessible_paths, ARGS[1]);
            if (storage_server_num != -1)
            {
                valid_path = true;
                printf(teal "[+]Path is present in storage server %d\n" white, storage_server_num + 1);
                printf(teal "[+]Sending command storage server details to the client\n" white);
                                
                // send that the file is found 
                bzero(client_gets, MAX_BUFFER_SIZE);
                strcpy(client_gets, "FOUND");
                if (send(clientSocket, client_gets, sizeof(client_gets), 0) < 0)
                {
                    perror(red "[-] Send" white);
                    exit(EXIT_FAILURE);
                }

                // send the storage server ip and port
                if (send(clientSocket, StorageServersInfo[storage_server_num].ip, sizeof(StorageServersInfo[storage_server_num].ip), 0) < 0)
                {
                    perror(red "[-] Send" white);
                    exit(EXIT_FAILURE);
                }

                if (send(clientSocket, &StorageServersInfo[storage_server_num].client_port, sizeof(StorageServersInfo[storage_server_num].client_port), 0) < 0)
                {
                    perror(red "[-] Send" white);
                    exit(EXIT_FAILURE);
                }

                // for this process, the communication with client and naming server should end here
            }

            // if the path is invalid
            if (!valid_path)
            {   
                printf(red "[-]Path not found -> 105\n" white);
                sprintf(client_gets, "Path isn't present in any of our storage servers. COMMAND: %s <PATH> <FILENAME>", ARGS[0]);
                if (send(clientSocket, client_gets, sizeof(client_gets), 0) < 0)
                {
                    perror(red "[-] Send" white);
                    exit(EXIT_FAILURE);
                }
            }
        }
        else if (strcasecmp(ARGS[0], "COPY") == 0)
        {
            command_type = 1;

            // send the command type to the cilent
            if (send(clientSocket, &command_type, sizeof(command_type), 0) < 0)
            {
                perror(red "[-] Send" white);
                exit(EXIT_FAILURE);
            }

            if (ARGS[1] == NULL || ARGS[2] == NULL)
            {
                // send an error message to the client
                sprintf(client_gets, "Invalid command. COMMAND: %s <PATH> <NEW PATH> -> 103", ARGS[0]);
                if (send(clientSocket, client_gets, sizeof(client_gets), 0) < 0)
                {
                    perror(red "[-] Send" white);
                    exit(EXIT_FAILURE);
                }
            }

            // check if the paths are valid
            bool valid_path = false;
            int storage_server_num1 = get_value(accessible_paths, ARGS[1]);
            int storage_server_num2 = get_value(accessible_paths, ARGS[2]);

            if (storage_server_num1 != -1 && storage_server_num2 != -1)
            {
                valid_path = true;
                printf(teal "[+]Path 1 is present in storage server %d\n" white, storage_server_num1 + 1);
                printf(teal "[+]Path 2 is present in storage server %d\n" white, storage_server_num2 + 1);   

                // here i just need a function which will facilitate the copy between the two storage servers

                // create sockets for both the storage servers along with the right arguments
                char TEMP_ARGS[2][PATH_MAX];
                strcpy(TEMP_ARGS[0], "SEND");
                strcpy(TEMP_ARGS[1], ARGS[1]);

                int storageSocket1 = connectToStorageServer(StorageServersInfo[storage_server_num1].ip, StorageServersInfo[storage_server_num1].naming_port, TEMP_ARGS, 2);

                strcpy(TEMP_ARGS[0], "RECEIVE");
                strcpy(TEMP_ARGS[1], ARGS[2]);

                int storageSocket2 = connectToStorageServer(StorageServersInfo[storage_server_num2].ip, StorageServersInfo[storage_server_num2].naming_port, TEMP_ARGS, 2);

                int result = facilitateCopy(storageSocket1, storageSocket2);

                if (result == 1)
                {   
                    // was a file 
                    // add entry to the list of accessible paths
                    char new_path[PATH_MAX*2];
                    snprintf(new_path, sizeof(new_path), "%s/%s", ARGS[2], basename(ARGS[1]));
                    printf(green "[+]Adding to list of accessible paths %s\n" white, new_path);
                    set_value(accessible_paths, new_path, get_value(accessible_paths, ARGS[2]));
                    printf(yellow "[+]Accessible paths\n");
                    print_map(accessible_paths);
                    printf(white);

                    // send acknowledgement to the client
                    bzero(client_gets, MAX_BUFFER_SIZE);
                    strcpy(client_gets, "ACK");
                    if (send(clientSocket, client_gets, sizeof(client_gets), 0) < 0)
                    {
                        perror(red "[-] Send" white);
                        exit(EXIT_FAILURE);
                    }

                    printf(green "[+]Operation was a success\n" white);
                }
                else if (result == 2)
                {
                    // was a folder 
                    // add entries to list of accessible paths
                    add_paths_to_list(accessible_paths, get_value(accessible_paths, ARGS[2]), ARGS[2], ARGS[1]);
                    printf(yellow "[+]Accessible paths\n");
                    print_map(accessible_paths);
                    printf(white);

                    // send acknowledgement to the client
                    bzero(client_gets, MAX_BUFFER_SIZE);
                    strcpy(client_gets, "ACK");
                    if (send(clientSocket, client_gets, sizeof(client_gets), 0) < 0)
                    {
                        perror(red "[-] Send" white);
                        exit(EXIT_FAILURE);
                    }

                    printf(green "[+]Operation was a success\n" white);
                }
                else
                {
                    // send acknowledgement to the client
                    bzero(client_gets, MAX_BUFFER_SIZE);
                    strcpy(client_gets, "FAIL");
                    if (send(clientSocket, client_gets, sizeof(client_gets), 0) < 0)
                    {
                        perror(red "[-] Send" white);
                        exit(EXIT_FAILURE);
                    }

                    printf(red "[-]Operation was a failure -> 104\n" white);
                }
            }

            if (!valid_path)
            {   
                printf(red "[-]Path not found -> 105\n" white);
                sprintf(client_gets, "Path isn't present in any of our storage servers. COMMAND: %s <PATH> <NEW PATH>", ARGS[0]);
                if (send(clientSocket, client_gets, sizeof(client_gets), 0) < 0)
                {
                    perror(red "[-] Send" white);
                    exit(EXIT_FAILURE);
                }
            }
        }
        else if (strcasecmp(ARGS[0], "DETAILS") == 0)
        {
            command_type = 4;
            // send the command type to the cilent
            if (send(clientSocket, &command_type, sizeof(command_type), 0) < 0)
            {
                perror(red "[-] Send" white);
                exit(EXIT_FAILURE);
            }

            if (ARGS[1] == NULL)
            {
                // send an error message to the client
                sprintf(client_gets, "Invalid command. COMMAND: %s <PATH> -> 103", ARGS[0]);
                if (send(clientSocket, client_gets, sizeof(client_gets), 0) < 0)
                {
                    perror(red "[-] Send" white);
                    exit(EXIT_FAILURE);
                }
            }

            // check if the path is valid
            bool valid_path = false;
            int storage_server_num = get_value(accessible_paths, ARGS[1]);
            if (storage_server_num != -1)
            {
                valid_path = true;
                printf(teal "[+]Path is present in storage server %d\n" white, storage_server_num + 1);
                printf(teal "[+]Sending command storage server details to the client\n" white);
                                
                // send that the file is found 
                bzero(client_gets, MAX_BUFFER_SIZE);
                strcpy(client_gets, "FOUND");
                if (send(clientSocket, client_gets, sizeof(client_gets), 0) < 0)
                {
                    perror(red "[-] Send" white);
                    exit(EXIT_FAILURE);
                }

                // send the storage server ip and port
                if (send(clientSocket, StorageServersInfo[storage_server_num].ip, sizeof(StorageServersInfo[storage_server_num].ip), 0) < 0)
                {
                    perror(red "[-] Send" white);
                    exit(EXIT_FAILURE);
                }

                if (send(clientSocket, &StorageServersInfo[storage_server_num].client_port, sizeof(StorageServersInfo[storage_server_num].client_port), 0) < 0)
                {
                    perror(red "[-] Send" white);
                    exit(EXIT_FAILURE);
                }

                // for this process, the communication with client and naming server should end here
            }

            // if the path is invalid
            if (!valid_path)
            {   
                printf(red "[-]Path not found -> 105\n" white);
                sprintf(client_gets, "Path isn't present in any of our storage servers. COMMAND: %s <PATH> <FILENAME>", ARGS[0]);
                if (send(clientSocket, client_gets, sizeof(client_gets), 0) < 0)
                {
                    perror(red "[-] Send" white);
                    exit(EXIT_FAILURE);
                }
            }
        }
        else if (strcasecmp(client_sends, "exit\n" white) == 0)
        {
            command_type = 5;
            // send the command type to the cilent
            if (send(clientSocket, &command_type, sizeof(command_type), 0) < 0)
            {
                perror(red "[-] Send" white);
                exit(EXIT_FAILURE);
            }

            printf(teal "Client %d has exited\n" white, client_id);
            // print client ip and port from the socket
            struct sockaddr_in clientAddr;
            socklen_t addr_size = sizeof(clientAddr);
            getpeername(clientSocket, (struct sockaddr *)&clientAddr, &addr_size);
            printf(yellow "[+]Client ip: %s\n" white, inet_ntoa(clientAddr.sin_addr));
            printf(yellow "[+]Client port: %d\n" white, ntohs(clientAddr.sin_port));
            break;
        }
        else
        {
            command_type = 6;
            // send the command type to the cilent
            if (send(clientSocket, &command_type, sizeof(command_type), 0) < 0)
            {
                perror(red "[-] Send" white);
                exit(EXIT_FAILURE);
            }

            printf(red "[-]Invalid command was sent by the client -> 102\n" white);
        }
    }

    // close socket
    printf(teal "[+]Closed socket for client %d\n" white, client_id);
    // print client ip and port from the socket
    struct sockaddr_in clientAddr;
    socklen_t addr_size = sizeof(clientAddr);
    getpeername(clientSocket, (struct sockaddr *)&clientAddr, &addr_size);
    printf(yellow "[+]Client ip: %s\n" white, inet_ntoa(clientAddr.sin_addr));
    printf(yellow "[+]Client port: %d\n" white, ntohs(clientAddr.sin_port));
    close(clientSocket);

    return NULL;
}

int main()
{
    accessible_paths = hashmap_create();
    LRU_Map= hashmap_create();

    lru = createLRU(LRU_Map, 3);

    // declare variables
    int namingServerSocket, otherSocket;
    struct sockaddr_in namingServerAddr, otherAddr;
    socklen_t addr_size;

    // create a socket for the main naming server
    namingServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (namingServerSocket < 0)
    {
        perror(red "[-]Socket" white);
        exit(EXIT_FAILURE);
    }

    // adding server address details to the naming server socket
    memset(&namingServerAddr, '\0', sizeof(namingServerAddr));
    namingServerAddr.sin_family = AF_INET;
    namingServerAddr.sin_addr.s_addr = inet_addr(ip);
    namingServerAddr.sin_port = htons(NAMING_SERVER_PORT);

    used_ports[0] = NAMING_SERVER_PORT;

    // Bind the socket to the address and port number.
    if (bind(namingServerSocket, (struct sockaddr *)&namingServerAddr, sizeof(namingServerAddr)) < 0)
    {
        perror(red "[-] Bind" white);
        exit(EXIT_FAILURE);
    }
    printf(green "[+]Binding successful with port %d\n" white, NAMING_SERVER_PORT);

    // listen for incoming connections from both storage servers and clients
    int allowed_backlog = 20;
    if (listen(namingServerSocket, allowed_backlog) < 0)
    {
        perror(red "[-]Listen" white);
        exit(EXIT_FAILURE);
    }
    printf(green "[+]Listening successful\n" white);

    while (1)
    {
        addr_size = sizeof(otherAddr);
        // Connecting to a given storage server or client
        otherSocket = accept(namingServerSocket, (struct sockaddr *)&otherAddr, &addr_size);
        if (otherSocket < 0)
        {
            perror(red "[-] Accept" white);
            exit(EXIT_FAILURE);
        }
        printf(green "[+]New connection accepted\n" white);
        printf(yellow "[+]Connection ip: %s\n" white, inet_ntoa(otherAddr.sin_addr));
        printf(yellow "[+]Connection port: %d\n" white, ntohs(otherAddr.sin_port));

        // receive the response about if it is a client server or a storage server connection
        bzero(naming_server_gets, MAX_BUFFER_SIZE);
        if (recv(otherSocket, naming_server_gets, sizeof(naming_server_gets), 0) < 0)
        {
            perror(red "[-] Recv" white);
            exit(EXIT_FAILURE);
        }

        // if it is a client connection
        if (strcasecmp(naming_server_gets, "c") == 0)
        {
            // create a new thread for the client
            bzero(naming_server_sends, MAX_BUFFER_SIZE);
            strcpy(naming_server_sends, "Initializing new thread for client connection\n" white);
            if (send(otherSocket, naming_server_sends, sizeof(naming_server_sends), 0) < 0)
            {
                perror(red "[-] Send" white);
                exit(EXIT_FAILURE);
            }

            // create a new thread for the client
            pthread_t thread_id;
            int *pclient = malloc(sizeof(int));
            *pclient = otherSocket;

            if (pthread_create(&thread_id, NULL, client_handler, (void *)pclient) < 0)
            {
                perror(red "[-] Thread" white);
                exit(EXIT_FAILURE);
            }

            printf(green "[+]Thread created for client connection\n" white);

            // detach the thread to save system resources
            pthread_detach(thread_id);
        }
        // otherwise if it is a storage server connection
        else if (strcasecmp(naming_server_gets, "s") == 0)
        {
            // send the initial setup information to the storage server
            bzero(naming_server_sends, MAX_BUFFER_SIZE);
            strcpy(naming_server_sends, "Initialising storage serevr with server info\n" white);
            if (send(otherSocket, naming_server_sends, sizeof(naming_server_sends), 0) < 0)
            {
                perror(red "[-] Send" white);
                exit(EXIT_FAILURE);
            }

            struct StorageServer storageServer;

            // receive storage server info
            bzero(&storageServer, sizeof(storageServer));
            if (recv(otherSocket, &storageServer, sizeof(storageServer), 0) < 0)
            {
                perror(red "[-] Recv" white);
                exit(EXIT_FAILURE);
            }

            // cheking if the given ports are available
            int port_available = 1;

            for (int i = 0; i < (storage_server_count * 2 + 1); i++)
            {
                if (storageServer.naming_port == used_ports[i] || storageServer.client_port == used_ports[i])
                {
                    port_available = 0;
                    break;
                }
            }

            if (port_available == 0)
            {
                bzero(naming_server_sends, MAX_BUFFER_SIZE);
                strcpy(naming_server_sends, "FAIL");
                if (send(otherSocket, naming_server_sends, sizeof(naming_server_sends), 0) < 0)
                {
                    perror(red "[-] Send" white);
                    exit(EXIT_FAILURE);
                }

                printf(red "[-]Port not available. Couldn't initialize storage server -> 106\n" white);

                // close socket
                printf(yellow "[+]Storage server ip: %s\n" white, inet_ntoa(otherAddr.sin_addr));
                printf(yellow "[+]Storage server port: %d\n" white, ntohs(otherAddr.sin_port));
                close(otherSocket);
                printf(teal "[+]Closed incoming storage server socket\n" white);
                // print ip and port
                continue;
            }

            // add storage server info to the array
            strcpy(StorageServersInfo[storage_server_count].ip, storageServer.ip);
            StorageServersInfo[storage_server_count].naming_port = storageServer.naming_port;
            StorageServersInfo[storage_server_count].client_port = storageServer.client_port;
            for (int i = 0; i < MAX_ACC_PATHS; i++)
            {
                strcpy(StorageServersInfo[storage_server_count].accessible_paths[i], storageServer.accessible_paths[i]);
                // add the accessible path to the hashmap as well along with the storage server where it is stored
                set_value(accessible_paths, storageServer.accessible_paths[i], storage_server_count);
                put_LRU(lru, storageServer.accessible_paths[i], storage_server_count);
            }
            printf(yellow "[+]Accessible paths\n");
            print_map(accessible_paths);
            printf(white);

            // increment storage server count
            storage_server_count++;
            used_ports[storage_server_count * 2 - 1] = storageServer.naming_port;
            used_ports[storage_server_count * 2] = storageServer.client_port;

            // send acknowledgement to storage server
            bzero(naming_server_sends, MAX_BUFFER_SIZE);
            strcpy(naming_server_sends, "ACK");
            if (send(otherSocket, naming_server_sends, sizeof(naming_server_sends), 0) < 0)
            {
                perror(red "[-] Send" white);
                exit(EXIT_FAILURE);
            }

            printf(green "[+]Storage server initialization is complete, stored information in the array\n" white);
            printf(teal "[+]Storage server count: %d\n" white, storage_server_count);

            // close socket
            close(otherSocket);
            printf(teal "[+]Closed incoming storage server socket\n" white);
        }
        // otherwise if it is an invalid input
        else
        {
            bzero(naming_server_sends, MAX_BUFFER_SIZE);
            strcpy(naming_server_sends, "Invalid input, please try again\n" white);
            if (send(otherSocket, naming_server_sends, sizeof(naming_server_sends), 0) < 0)
            {
                perror(red "[-] Send" white);
                exit(EXIT_FAILURE);
            }

            // close socket
            close(otherSocket);
            printf(teal "[+]Closed the incoming socket\n" white);
        }
    }

    // close socket
    printf(teal "[+]Closing naming server socket\n" white);
    close(namingServerSocket);
    return 0;
}