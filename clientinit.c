#include "definitions.h"

char *ip = "127.0.0.1";

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
    // printf(teal "[+]Processed %d arguments\n" white, *no_args);

    return 0;
}

int connectToStorageServer(char storage_server_ip[], int storage_server_port, char command[])
{
    // connect to storage server
    int storageServerSocket;
    struct sockaddr_in storageServerAddress;

    storageServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (storageServerSocket < 0)
    {
        perror(red "[-] Socket" white);
        exit(EXIT_FAILURE);
    }

    // setting up the address to which the connections for storage servers happen
    memset(&storageServerAddress, '\0', sizeof(storageServerAddress));
    storageServerAddress.sin_family = AF_INET;
    storageServerAddress.sin_addr.s_addr = inet_addr(storage_server_ip);
    storageServerAddress.sin_port = htons(storage_server_port);

    // connect to storage server
    if (connect(storageServerSocket, (struct sockaddr *)&storageServerAddress, sizeof(storageServerAddress)) < 0)
    {
        perror(red "[-] Connect" white);
        exit(EXIT_FAILURE);
    }
    printf(green "[+]Connected to storage server\n" white);

    char ARGS[MAX_ARGS][PATH_MAX];
    int no_args;

    // get the arguments from the command
    processCommand(command, ARGS, &no_args);

    // send the number of args to the storage server
    if (send(storageServerSocket, &no_args, sizeof(no_args), 0) < 0)
    {
        perror(red "[-] Send" white);
        exit(EXIT_FAILURE);
    }

    char client_gets[MAX_BUFFER_SIZE];
    char client_sends[MAX_BUFFER_SIZE];

    // send the arguments to the storage server
    for (int i = 0; i < no_args; i++)
    {
        printf(teal "[+]Sending %s to storage server\n" white, ARGS[i]);

        if (send(storageServerSocket, ARGS[i], sizeof(ARGS[i]), 0) < 0)
        {
            perror(red "[-] Send" white);
            exit(EXIT_FAILURE);
        }

        // receive acknowledgement from storage server
        bzero(client_gets, MAX_BUFFER_SIZE);
        if (recv(storageServerSocket, client_gets, sizeof(client_gets), 0) < 0)
        {
            perror(red "[-] Recv" white);
            exit(EXIT_FAILURE);
        }

        // check if the acknowledgement is correct
        if (strcasecmp(client_gets, "ACK") != 0)
        {
            printf(red "[-]Error sending command to storage server\n" white);
            exit(EXIT_FAILURE);
        }
    }

    // send final done packet to storage server
    bzero(client_sends, MAX_BUFFER_SIZE);
    strcpy(client_sends, "DONE");
    if (send(storageServerSocket, client_sends, sizeof(client_sends), 0) < 0)
    {
        perror(red "[-] Send" white);
        exit(EXIT_FAILURE);
    }

    // return the socket to the storage server
    return storageServerSocket;
}

// 1 for success, 0 for failure
int readClientSide(int storageServerSocket)
{
    // start receiving packets from the storage server and keep doing it untill you receive a stop packet
    char client_gets[CHUNK_SIZE + 1];
    char client_sends[CHUNK_SIZE + 1];

    while (1)
    {
        // receive the packet from the storage server
        bzero(client_gets, CHUNK_SIZE + 1);
        if (recv(storageServerSocket, client_gets, sizeof(client_gets), 0) < 0)
        {
            perror(red "[-] Recv" white);
            return 0;
        }

        // send acknowledgement to the storage server
        bzero(client_sends, CHUNK_SIZE + 1);
        strcpy(client_sends, "ACK");
        if (send(storageServerSocket, client_sends, sizeof(client_sends), 0) < 0)
        {
            perror(red "[-] Send" white);
            return 0;
        }

        // check if the packet is a stop packet
        if (strcasecmp(client_gets, "STOP") == 0)
        {
            printf(green "[+]Received STOP packet from storage server\n" white);
            break;
        }

        // print the packet
        printf(white "[+]Received packet: %s\n" white, client_gets);
    }

    // close the socket to the storage server
    close(storageServerSocket);
    printf(teal "[+]Closed storage server socket\n" white);

    return 1;
}

int writeClientSide(int storageServerSocket)
{
    // input the file path that the client wants to write from
    char input_file[PATH_MAX];
    printf(pink "[+]Enter the relative path of the file that you want to write from (FORMAT: ./path): " white);
    // scanf("%[^\n]s", input_file);
    // printf(teal "[+]File path length: %ld\n" white, strlen(input_file));
    fgets(input_file, PATH_MAX, stdin);

    // open the file
    char file_name[PATH_MAX];
    char _;
    sscanf(input_file, "%s%c", file_name, &_);
    FILE *file_read = fopen(file_name, "r");

    if (file_read == NULL)
    {
        printf(red "[-]Error opening file\n" white);
        //close the socket to the storage server
        close(storageServerSocket);
        printf(teal "[+]Closed storage server socket\n" white);
        return 0;
    }

    long long int bytes_read = 0;

    char client_sends[CHUNK_SIZE + 1];
    char client_gets[CHUNK_SIZE + 1];

    // read the file in chunks of CHUNK_SIZE and send it to the storage server
    while ((bytes_read = fread(client_sends, sizeof(char), CHUNK_SIZE, file_read)) > 0)
    {
        // send the chunk to the storage server
        if (send(storageServerSocket, client_sends, bytes_read, 0) < 0)
        {
            perror(red "[-] Send" white);
            return 0;
        }

        // receive acknowledgement from the storage server
        bzero(client_gets, CHUNK_SIZE + 1);
        if (recv(storageServerSocket, client_gets, sizeof(client_gets), 0) < 0)
        {
            perror(red "[-] Recv" white);
            return 0;
        }

        // check if the acknowledgement is correct
        if (strcasecmp(client_gets, "ACK") != 0)
        {
            printf(red "[-]Error sending chunk to storage server\n" white);
            return 0;
        }
    }

    // send stop packet to the storage server
    bzero(client_sends, CHUNK_SIZE + 1);
    strcpy(client_sends, "STOP");
    if (send(storageServerSocket, client_sends, sizeof(client_sends), 0) < 0)
    {
        perror(red "[-] Send" white);
        return 0;
    }

    // close the file
    fclose(file_read);

    // close the socket to the storage server
    close(storageServerSocket);
    printf(teal "[+]Closed storage server socket\n" white);

    return 1;    
}

int fileDetailsClientSide(int storageServerSocket)
{
    // receive the file details from the storage server
    char client_gets[MAX_BUFFER_SIZE];
    char client_sends[MAX_BUFFER_SIZE];

    // get the file details string from the storage server
    bzero(client_gets, MAX_BUFFER_SIZE);
    if (recv(storageServerSocket, client_gets, sizeof(client_gets), 0) < 0)
    {
        perror(red "[-] Recv" white);
        return 0;
    }

    // print the file details string in the format that the client wants

    char file_name[PATH_MAX];
    off_t file_size;
    mode_t file_perms;
    uid_t file_owner;
    gid_t file_group;
    time_t last_modified_time;
    char time[50];

    sscanf(client_gets, "%s %ld %o %d %d %ld", file_name, &file_size, &file_perms, &file_owner, &file_group, &last_modified_time);
    strcpy(time, ctime(&last_modified_time));

    printf(green "[+]File details:\n" white);
    printf(green "\tFile name: %s\n" white, file_name);
    printf(green "\tFile size: %ld\n" white, file_size);
    printf(green "\tFile permissions: %o\n" white, file_perms);
    printf(green "\tFile owner: %d\n" white, file_owner);
    printf(green "\tFile group: %d\n" white, file_group);
    printf(green "\tLast modified time: %s", time);

    // send acknowledgement to the storage server
    bzero(client_sends, MAX_BUFFER_SIZE);
    strcpy(client_sends, "ACK");
    if (send(storageServerSocket, client_sends, sizeof(client_sends), 0) < 0)
    {
        perror(red "[-] Send" white);
        return 0;
    }

    // close the socket to the storage server
    close(storageServerSocket);
    printf(teal "[+]Closed storage server socket\n" white);

    return 1;
}

int main()
{
    // connect to naming server
    int namingServerSocket;
    struct sockaddr_in namingServerAddress;

    namingServerSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (namingServerSocket < 0)
    {
        perror(red "[-] Socket" white);
        exit(EXIT_FAILURE);
    }

    // setting up the address to which the connections for storage servers happen
    memset(&namingServerAddress, '\0', sizeof(namingServerAddress));
    namingServerAddress.sin_family = AF_INET;
    namingServerAddress.sin_addr.s_addr = inet_addr(ip);
    namingServerAddress.sin_port = htons(NAMING_SERVER_PORT);

    // connect to naming server
    if (connect(namingServerSocket, (struct sockaddr *)&namingServerAddress, sizeof(namingServerAddress)) < 0)
    {
        perror(red "[-] Connect" white);
        exit(EXIT_FAILURE);
    }
    printf(green "[+]Connected to naming server\n" white);

    char client_gets[MAX_BUFFER_SIZE];
    char client_sends[MAX_BUFFER_SIZE];

    // send the fact that this is a client to the naming server
    bzero(client_sends, MAX_BUFFER_SIZE);
    strcpy(client_sends, "c");
    if (send(namingServerSocket, client_sends, sizeof(client_sends), 0) < 0)
    {
        perror(red "[-] Send" white);
        exit(EXIT_FAILURE);
    }

    // receive acknowledgement from naming server
    bzero(client_gets, MAX_BUFFER_SIZE);
    if (recv(namingServerSocket, client_gets, sizeof(client_gets), 0) < 0)
    {
        perror(red "[-] Recv" white);
        exit(EXIT_FAILURE);
    }
    printf(green "[+]Naming server sent: %s", client_gets);

    while (1)
    {
        // get input from the user about what they want to do
        printf(pink "[+]Enter your command: " white);
        bzero(client_sends, MAX_BUFFER_SIZE);
        fgets(client_sends, MAX_BUFFER_SIZE, stdin);

        if (strcasecmp(client_sends, "\n" white) == 0)
        {
            printf(red "[-] Empty command\n" white);
            continue;
        }

        // send the command to the naming server
        if (send(namingServerSocket, client_sends, sizeof(client_sends), 0) < 0)
        {
            perror(red "[-] Send" white);
            exit(EXIT_FAILURE);
        }

        int command_type = 0;

        // receive the command type from the naming server in the command type variable
        if (recv(namingServerSocket, &command_type, sizeof(command_type), 0) < 0)
        {
            perror(red "[-] Recv" white);
            exit(EXIT_FAILURE);
        }

        if (command_type == 1)
        {
            // this means that the client would need to listen to one response from the naming server about the execution of the process 

            bzero(client_gets, MAX_BUFFER_SIZE);
            if (recv(namingServerSocket, client_gets, sizeof(client_gets), 0) < 0)
            {
                perror(red "[-] Recv" white);
                exit(EXIT_FAILURE);
            }

            printf(teal "[+]Naming server sent: %s\n" white, client_gets);

            if (strcasecmp(client_gets, "ACK") == 0)
            {
                printf(green "[+]Successfully executed command\n" white);
            }
            else
            {
                printf(red "[-]Error executing command.\n" white);
            }
        }
        else if (command_type == 2 || command_type == 3)
        {
            // this means it's a read or write command, client will get storage server details from the naming server and then connect to the storage server and get the file
            
            // receive if the file is found otherwise receive error message from naming server 
            bzero(client_gets, MAX_BUFFER_SIZE);
            if (recv(namingServerSocket, client_gets, sizeof(client_gets), 0) < 0)
            {
                perror(red "[-] Recv" white);
                exit(EXIT_FAILURE);
            }

            if (strcasecmp(client_gets, "FOUND") == 0)
            {
                printf(green "[+]File found, getting storage server details\n" white);

                char storage_server_ip[IP_LEN];
                int storage_server_port;

                // receive the storage server ip and port
                bzero(storage_server_ip, IP_LEN);
                if (recv(namingServerSocket, storage_server_ip, sizeof(storage_server_ip), 0) < 0)
                {
                    perror(red "[-] Recv" white);
                    exit(EXIT_FAILURE);
                }

                if (recv(namingServerSocket, &storage_server_port, sizeof(storage_server_port), 0) < 0)
                {
                    perror(red "[-] Recv" white);
                    exit(EXIT_FAILURE);
                }

                // connect to storage server and get the socket to the storage server
                int storageServerSocket = connectToStorageServer(storage_server_ip, storage_server_port, client_sends);

                if (command_type == 2)
                {
                    // client asked for read
                    printf(teal "[+]Reading file from storage server\n" white);

                    int readSuccess = readClientSide(storageServerSocket);

                    if (readSuccess == 0)
                    {
                        printf(red "[-]Error reading file from storage server\n" white);
                    }
                    else
                    {
                        printf(green "[+]Successfully read file from storage server\n" white);
                    }
                }
                else 
                {
                    // client asked to write
                    printf(teal "[+]Writing file to storage server\n" white);

                    int writeSuccess = writeClientSide(storageServerSocket);

                    if (writeSuccess == 0)
                    {
                        printf(red "[-]Error writing file to storage server\n" white);
                    }
                    else
                    {
                        printf(green "[+]Successfully wrote file to storage server\n" white);
                    }
                }   
            }
            else
            {
                printf(red "[-] Naming server sent: %s\n" white, client_gets);
            }
        }
        else if (command_type == 4)
        {
            // this is for getting details about a file 
            // receive if the file is found otherwise receive error message from naming server
            bzero(client_gets, MAX_BUFFER_SIZE);
            if (recv(namingServerSocket, client_gets, sizeof(client_gets), 0) < 0)
            {
                perror(red "[-] Recv" white);
                exit(EXIT_FAILURE);
            }

            if (strcasecmp(client_gets, "FOUND") == 0)
            {
                printf(green "[+]File found, getting storage server details\n" white);

                char storage_server_ip[IP_LEN];
                int storage_server_port;

                // receive the storage server ip and port
                bzero(storage_server_ip, IP_LEN);
                if (recv(namingServerSocket, storage_server_ip, sizeof(storage_server_ip), 0) < 0)
                {
                    perror(red "[-] Recv" white);
                    exit(EXIT_FAILURE);
                }

                if (recv(namingServerSocket, &storage_server_port, sizeof(storage_server_port), 0) < 0)
                {
                    perror(red "[-] Recv" white);
                    exit(EXIT_FAILURE);
                }

                // connect to storage server and get the socket to the storage server
                int storageServerSocket = connectToStorageServer(storage_server_ip, storage_server_port, client_sends);

                printf(teal "[+]Getting file details from storage server\n" white);

                int result = fileDetailsClientSide(storageServerSocket);

                if (result == 0)
                {
                    printf(red "[-]Error getting file details from storage server\n" white);
                }
                else
                {
                    printf(green "[+]Successfully got file details from storage server\n" white);
                }
            }
            else
            {
                printf(red "[-] Naming server sent: %s\n" white, client_gets);
            }
        }
        else if (command_type == 5)
        {
            // client asked for exit 
            printf(green "[+]Exiting\n" white);
            break;
        }
        else if (command_type == 6)
        {
            // incorrect command
            printf(red "[-]Incorrect command - commad doesn't exist\n" white);
        }
    }

    close(namingServerSocket);
    printf(teal "[+]Closed naming server socket\n" white);

    return 0;
}