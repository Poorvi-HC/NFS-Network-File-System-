// include definitions
#include "definitions.h"
#include "hashmap/map_operations.h"
#include "operations/create_file.h"
#include "operations/delete.h"

char *ip = "127.0.0.1";
char storage_server_path[PATH_MAX];
int it_array = 0;
int naming_port = 0;
int client_port = 0;
int associated_files = 0;

hashmap *path_to_sem = NULL;

// array of semaphores
sem_t semaphores[MAX_ASSOCIATED_FILES];
sem_t semlocks[MAX_ASSOCIATED_FILES];

int reader_count[MAX_ASSOCIATED_FILES];

typedef struct rw_thread_args
{
	int socket;
	char path[PATH_MAX];
	int sem_index;
} rw_thread_args;

void *read_file(void *arg)
{
	char server_sends[CHUNK_SIZE + 1];
	char server_gets[CHUNK_SIZE + 1];

	rw_thread_args *args = (rw_thread_args *)arg;
	int client_socket = args->socket;
	char path[PATH_MAX + 2];
	sprintf(path, ".%s", args->path);

	// increase the reader count so that no file can write to it when someone is reading from it
	sem_wait(&semlocks[args->sem_index]);
	reader_count[args->sem_index]++;
	if (reader_count[args->sem_index] > 0)
	{
		sem_wait(&semaphores[args->sem_index]);
	}
	sem_post(&semlocks[args->sem_index]);

	FILE *file_read = fopen((char *)path, "rb");

	if (file_read == NULL)
	{
		perror(red "[-] File open" white);
		exit(EXIT_FAILURE);
	}

	// fseek(file_read, 0, SEEK_END);
	// long file_size = ftell(file_read);
	// fseek(file_read, 0, SEEK_SET);

	// send the file to the client
	long long int bytes_read;
	while ((bytes_read = fread(server_sends, sizeof(char), CHUNK_SIZE, file_read)) > 0)
	{
		// send the data to the client
		if (send(client_socket, server_sends, bytes_read, 0) < 0)
		{
			perror(red "[-] Send" white);
			exit(EXIT_FAILURE);
		}

		// receive acknowledgement from the client
		bzero(server_gets, CHUNK_SIZE + 1);
		if (recv(client_socket, server_gets, CHUNK_SIZE + 1, 0) < 0)
		{
			perror(red "[-] Recv" white);
			exit(EXIT_FAILURE);
		}

		// check if the client has sent proper acknowledgement
		if (strcasecmp(server_gets, "ACK") != 0)
		{
			printf(red "[-] Invalid acknowledgement\n" white);
			exit(EXIT_FAILURE);
		}
	}

	// send stop packet when the entire file has been sent
	bzero(server_sends, CHUNK_SIZE + 1);
	strcpy(server_sends, "STOP");
	if (send(client_socket, server_sends, CHUNK_SIZE + 1, 0) < 0)
	{
		perror(red "[-] Send" white);
		exit(EXIT_FAILURE);
	}

	fclose(file_read);

	// decrease the reader count allowing it to be open for writing
	sem_wait(&semlocks[args->sem_index]);
	reader_count[args->sem_index]--;
	if (reader_count[args->sem_index] == 0)
	{
		sem_post(&semaphores[args->sem_index]);
	}
	sem_post(&semlocks[args->sem_index]);

	// close the socket to the client
	close(client_socket);
	printf(teal "[+]Closed client socket\n" white);

	pthread_exit(NULL);
	return NULL;
}

// return 1 on success and 0 on failure
int readServerSide(char ARGS[][PATH_MAX], int no_args, int client_socket)
{
	if (no_args != 2)
	{
		printf(red "[-] Invalid number of arguments. COMMAND: READ <PATH>\n" white);
		return 0;
	}

	// add the arguments to the structure
	rw_thread_args args;
	args.socket = client_socket;
	strcpy(args.path, ARGS[1]);

	// find the associated semaphore for the given file
	int sem_index = get_value(path_to_sem, ARGS[1]);

	if (sem_index == -1)
	{
		printf(red "[-] File not found\n" white);
		return 0;
	}

	args.sem_index = sem_index;

	// create a thread for reading the file
	pthread_t readThread;
	if (pthread_create(&readThread, NULL, read_file, (void *)&args) != 0)
	{
		perror(red "[-] Pthread create" white);
		return 0;
	}

	// joining the thread so that the results are obtained only when the entire process is completed
	if (pthread_join(readThread, NULL) != 0)
	{
		perror(red "[-] Pthread join" white);
		return 0;
	}

	// as the thread has completed successfully, return code for success
	return 1;
}

void *write_to_file(void *arg)
{
	char server_sends[CHUNK_SIZE + 1];
	char server_gets[CHUNK_SIZE + 1];

	rw_thread_args *args = (rw_thread_args *)arg;
	int client_socket = args->socket;
	char path[PATH_MAX + 2];
	sprintf(path, ".%s", args->path);
	int sem_index = args->sem_index;

	FILE *file_write = fopen((char *)path, "wb");

	if (file_write == NULL)
	{
		perror(red "[-] File open" white);
		exit(EXIT_FAILURE);
	}

	sem_wait(&semaphores[sem_index]);
	while (1)
	{
		// receive the data from the client
		bzero(server_gets, CHUNK_SIZE + 1);
		if (recv(client_socket, server_gets, CHUNK_SIZE + 1, 0) < 0)
		{
			perror(red "[-] Recv" white);
			exit(EXIT_FAILURE);
		}

		// send acknowledgement to the client
		bzero(server_sends, CHUNK_SIZE + 1);
		strcpy(server_sends, "ACK");
		if (send(client_socket, server_sends, CHUNK_SIZE + 1, 0) < 0)
		{
			perror(red "[-] Send" white);
			exit(EXIT_FAILURE);
		}

		// check if the stop packet has been received
		if (strcasecmp(server_gets, "STOP") == 0)
		{
			break;
		}

		// write the data to the file
		if (fwrite(server_gets, sizeof(char), strlen(server_gets), file_write) < 0)
		{
			perror(red "[-] Fwrite" white);
			exit(EXIT_FAILURE);
		}
	}
	sem_post(&semaphores[sem_index]);

	fclose(file_write);

	// close the socket to the client
	close(client_socket);
	printf(teal "[+]Closed client socket\n" white);

	pthread_exit(NULL);
	return NULL;
}

int writeServerSide(char ARGS[][PATH_MAX], int no_args, int client_socket)
{
	if (no_args != 2)
	{
		printf(red "[-] Invalid number of arguments. COMMAND: WRITE <PATH>\n" white);
		return 0;
	}

	// add the arguments to the structure
	rw_thread_args args;
	args.socket = client_socket;
	strcpy(args.path, ARGS[1]);

	// create a thread for writing to the file
	pthread_t writeThread;

	// find the associated semaphore for the given file
	int sem_index = get_value(path_to_sem, ARGS[1]);

	if (sem_index == -1)
	{
		printf(red "[-] File not found\n" white);
		return 0;
	}

	args.sem_index = sem_index;

	// sem_init(&semaphores[sem_index], 0, 1);
	// sem_init(&semlocks[sem_index], 0, 1);
	// reader_count[sem_index] = 0;

	if (pthread_create(&writeThread, NULL, write_to_file, (void *)&args) != 0)
	{
		perror(red "[-] Pthread create" white);
		return 0;
	}

	// joining the thread so that the results are obtained only when the entire process is completed
	if (pthread_join(writeThread, NULL) != 0)
	{
		perror(red "[-] Pthread join" white);
		return 0;
	}

	// as the thread has completed successfully, return code for success
	return 1;
}

void *get_file_details(void* arg)
{
	// the file details are sent in the following format
	// <file_name> <file_size> <file_permissions> <file_owner> <file_group> <file_last_modified_time>

	char server_sends[MAX_BUFFER_SIZE];
	char server_gets[MAX_BUFFER_SIZE];

	rw_thread_args *args = (rw_thread_args *)arg;
	int client_socket = args->socket;
	char path[PATH_MAX + 2];
	sprintf(path, ".%s", args->path);

	struct stat file_stat;

	// get the file details using stat function
	if (stat(path, &file_stat) == -1) {
		perror(red "[-] Failed to get file details" white);
		exit(EXIT_FAILURE);
	}

	// extract the file details from the file_stat structure
	char file_name[PATH_MAX];
	strcpy(file_name, basename(path));
	off_t file_size = file_stat.st_size;
	mode_t file_permissions = file_stat.st_mode;
	uid_t file_owner = file_stat.st_uid;
	gid_t file_group = file_stat.st_gid;
	time_t file_last_modified_time = file_stat.st_mtime;

	// format the file details string
	sprintf(server_sends, "%s %ld %o %d %d %ld", file_name, file_size, file_permissions, file_owner, file_group, file_last_modified_time);

	// send the file details to the client
	if (send(client_socket, server_sends, strlen(server_sends), 0) < 0) {
		perror(red "[-] Failed to send file details to client" white);
		exit(EXIT_FAILURE);
	}

	// receive acknowledgement from the client
	if (recv(client_socket, server_gets, sizeof(server_gets), 0) < 0)
	{
		perror(red "[-] Recv" white);
		exit(EXIT_FAILURE);
	}

	return NULL;
}

int fileDetailsServerSide(char ARGS[][PATH_MAX], int no_args, int client_socket)
{
	if (no_args != 2)
	{
		printf(red "[-] Invalid number of arguments. COMMAND: DETAILS <PATH>\n" white);
		return 0;
	}

	rw_thread_args args;
	args.socket = client_socket;
	strcpy(args.path, ARGS[1]);

	// create a thread for getting file details and sending it to the client so that it is a non blocking operation
	pthread_t detailsThread;

	if (pthread_create(&detailsThread, NULL, get_file_details, (void *)&args) != 0)
	{
		perror(red "[-] Pthread create" white);
		return 0;
	}

	// joining the thread so that the results are obtained only when the entire process is completed
	if (pthread_join(detailsThread, NULL) != 0)
	{
		perror(red "[-] Pthread join" white);
		return 0;
	}

	// as the thread has completed successfully, return code for success
	return 1;
}

void *send_file(void *arg)
{
	char server_sends[CHUNK_SIZE + 1];

	rw_thread_args *args = (rw_thread_args *)arg;
	int naming_socket = args->socket;
	char path[PATH_MAX + 2];
	sprintf(path, ".%s", args->path);

	// get the stats for the file
	struct stat file_stat;
	if (stat(path, &file_stat) == -1)
	{
		perror(red "[-] Failed to get file details" white);
		exit(EXIT_FAILURE);
	}

	// check if it is a directory
	int dir = S_ISDIR(file_stat.st_mode);

	// zip it if it is a directory
	if (dir)
	{
		char zip_command[PATH_MAX * 3];
		sprintf(zip_command, "tar -czvf %s.tar.gz %s", path, path);
		system(zip_command);
		strcat(path, ".tar.gz");
	}

	// get the name of the file
	char file_name[PATH_MAX];
	strcpy(file_name, basename(path));

	// send the file name to the naming server
	if (send(naming_socket, file_name, strlen(file_name), 0) < 0)
	{
		perror(red "[-] Send" white);
		exit(EXIT_FAILURE);
	}
	printf(teal "[+]Sent file name: %s\n" white, file_name);

	// open the file
	FILE *file_read = fopen((char *)path, "rb");

	if (file_read == NULL)
	{
		perror(red "[-] File open" white);
		exit(EXIT_FAILURE);
	}

	long long int bytes_read;
	// read the file and send it to the client 
	while ((bytes_read = fread(server_sends, sizeof(char), CHUNK_SIZE, file_read)) > 0)
	{
		// printf(green "[+]Sending chunk of size %lld\n" white, bytes_read);
		// send the data to the naming server
		if (send(naming_socket, server_sends, bytes_read, 0) < 0)
		{
			perror(red "[-] Send" white);
			exit(EXIT_FAILURE);
		}
	}
	
	// waiting so that naming server can properly receive the STOP packet
	sleep(1);
	
	// send STOP packet when the entire file has been sent
	bzero(server_sends, CHUNK_SIZE + 1);
	strcpy(server_sends, "STOP");
	printf(teal "[+]Sending stop packet to naming server %s\n" white, server_sends);
	if (send(naming_socket, server_sends, CHUNK_SIZE + 1, 0) < 0)
	{
		perror(red "[-] Send");
		exit(EXIT_FAILURE);	
	}
	printf(green "[+]Sent STOP packet\n" white);

	fclose(file_read);
	
	// remove the zip file 
	if (dir)
	{
		char remove_command[PATH_MAX * 2];
		sprintf(remove_command, "rm %s", file_name);
		printf(teal "[+]Removing file: %s\n" white, remove_command);
		system(remove_command);
	}

	pthread_exit(NULL);
	return NULL;
}

int runSend(char ARGS[][PATH_MAX], int no_args, int naming_socket)
{
	if (no_args != 2)
	{
		printf(red "[-] Invalid number of arguments. COMMAND: SEND <PATH>\n" white);
		return 0;
	}

	// add the arguments to the structure
	rw_thread_args args;
	strcpy(args.path, ARGS[1]);
	args.socket = naming_socket;

	// create a thread for sending the file
	pthread_t sendThread;
	if (pthread_create(&sendThread, NULL, send_file, (void *)&args) != 0)
	{
		perror(red "[-] Pthread create" white);
		return 0;
	}

	// joining the thread so that the results are obtained only when the entire process is completed
	if (pthread_join(sendThread, NULL) != 0)
	{
		perror(red "[-] Pthread join" white);
		return 0;
	}

	// as the thread has completed successfully, return code for success
	return 1;
}

void *receive_file(void *arg)
{
	char server_gets[CHUNK_SIZE + 1];

	rw_thread_args *args = (rw_thread_args *)arg;
	int naming_socket = args->socket;
	char path[PATH_MAX + 2];
	sprintf(path, ".%s", args->path);

	// get the name of the file from the naming server
	bzero(server_gets, CHUNK_SIZE + 1);
	if (recv(naming_socket, server_gets, CHUNK_SIZE + 1, 0) < 0)
	{
		perror(red "[-] Recv" white);
		exit(EXIT_FAILURE);
	}
	printf(teal "[+]Received file name: %s\n" white, server_gets);

	// check if it has .tar.gz at the end
	int zip = 0;
	if (strcmp(server_gets + strlen(server_gets) - 7, ".tar.gz") == 0)
	{
		zip = 1;
	}

	// create the complete file path
	char file_path_name[PATH_MAX + 515];
	char file_name[PATH_MAX];
	strcpy(file_name, server_gets);
	sprintf(file_path_name, "%s/%s", path, server_gets);
	printf(green "[+]File path name: %s\n" white, file_path_name);

	// open the file
	FILE *file_write = fopen((char *)file_path_name, "wb");

	if (file_write == NULL)
	{
		perror(red "[-] File open" white);
		exit(EXIT_FAILURE);
	}

	// receive the data from naming server and write it to the file
	while (1)
	{
		// printf(green "[+]Receiving chunk\n" white);
		// receive the data from the naming server
		bzero(server_gets, CHUNK_SIZE + 1);
		if (recv(naming_socket, server_gets, CHUNK_SIZE + 1, 0) < 0)
		{
			perror(red "[-] Recv" white);
			exit(EXIT_FAILURE);
		}

		// check if the stop packet has been received
		if (strcasecmp(server_gets, "STOP") == 0)
		{
			printf(green "[+]Received STOP packet\n" white);
			break;
		}

		// write the data to the file
		if (fwrite(server_gets, sizeof(char), strlen(server_gets), file_write) < 0)
		{
			perror(red "[-] Fwrite" white);
			exit(EXIT_FAILURE);
		}
	}

	// send if it was a file or a directory
	if (send(naming_socket, &zip, sizeof(zip), 0) < 0)
	{
		perror(red "[-] Send");
		exit(EXIT_FAILURE);
	}

	fclose(file_write);

	// extract the zip file here
	if (zip)
	{
		char unzip_command[PATH_MAX * 3];
		sprintf(unzip_command, "tar -xzf %s -C %s", file_path_name, path);
		printf(green "[+]Unzipping file: %s\n" white, unzip_command);
		system(unzip_command);
	}

	pthread_exit(NULL);
	return NULL;
}

int runReceive(char ARGS[][PATH_MAX], int no_args, int naming_socket)
{
	if (no_args != 2)
	{
		printf(red "[-] Invalid number of arguments. COMMAND: RECEIVE <PATH>\n" white);
		return 0;
	}

	// add the arguments to the structure
	rw_thread_args args;
	strcpy(args.path, ARGS[1]);
	args.socket = naming_socket;

	// create a thread for receiving the file
	pthread_t receiveThread;
	if (pthread_create(&receiveThread, NULL, receive_file, (void *)&args) != 0)
	{
		perror(red "[-] Pthread create" white);
		return 0;
	}

	// joining the thread so that the results are obtained only when the entire process is completed
	if (pthread_join(receiveThread, NULL) != 0)
	{
		perror(red "[-] Pthread join" white);
		return 0;
	}

	// as the thread has completed successfully, return code for success
	return 1;
}

int execClientCommand(char ARGS[][PATH_MAX], int no_args, int client_socket)
{
	if (strcasecmp(ARGS[0], "READ") == 0)
	{
		return readServerSide(ARGS, no_args, client_socket);
	}
	else if (strcasecmp(ARGS[0], "WRITE") == 0)
	{
		return writeServerSide(ARGS, no_args, client_socket);
	}
	else if (strcasecmp(ARGS[0], "DETAILS") == 0)
	{
		return fileDetailsServerSide(ARGS, no_args, client_socket);
	}
	else
	{
		return -1;
	}
}

// run functions for each operation
// return 1 on success
// return -1 on invalid command/arguments
// return 0 on error
int execCommand(char ARGS[][PATH_MAX], int no_args, int namingServerSocket)
{
    if (strcasecmp(ARGS[0], "CREATE") == 0)
    {
        return runCreate(ARGS, no_args);
    }
    else if (strcasecmp(ARGS[0], "DELETE") == 0)
    {
        return runDelete(ARGS, no_args);
    }
	else if (strcasecmp(ARGS[0], "SEND") == 0)
	{
		return runSend(ARGS, no_args, namingServerSocket);
	}
	else if (strcasecmp(ARGS[0], "RECEIVE") == 0)
	{
		return runReceive(ARGS, no_args, namingServerSocket);
	}
    else
    {
        return -1;
    }
}

void getFiles(char *path, char *files[])
{
	struct dirent *de;
	DIR *dr = opendir(path);

	if (dr == NULL)
	{
		printf(red "[-] Could not open current directory\n" white);
		return;
	}

	while ((de = readdir(dr)) != NULL)
	{
		if (strcasecmp(de->d_name, ".") != 0 && strcasecmp(de->d_name, "..") != 0)
		{
			// adding the file name to the array
			char file_path_name[PATH_MAX];
			strcpy(file_path_name, path);
			strcat(file_path_name, "/");
			strcat(file_path_name, de->d_name);

			// making sure the file path name is relative to the storage server path
			strcpy(file_path_name, file_path_name + strlen(storage_server_path));
			strcpy(files[it_array], file_path_name);
			it_array++;

			// check if it is a directory
			if (de->d_type == DT_DIR)
			{
				// calling the same function for the sub directory
				char new_path[PATH_MAX];
				strcpy(new_path, path);
				strcat(new_path, "/");
				strcat(new_path, de->d_name);
				getFiles(new_path, files);
			}
		}
	}

	closedir(dr);
}

void *portThread(void *portNum)
{
	int port = *(int *)portNum;
	free(portNum);

	int storageServerSocket, incomingSocket;
	struct sockaddr_in storageServerAddress, incomingAddress;
	socklen_t addr_size;

	storageServerSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (storageServerSocket < 0)
	{
		perror(red "[-] Socket");
		exit(EXIT_FAILURE);
	}

	// setting up the address to which the connections for storage servers happen
	memset(&storageServerAddress, '\0', sizeof(storageServerAddress));
	storageServerAddress.sin_family = AF_INET;
	storageServerAddress.sin_addr.s_addr = inet_addr(ip);
	storageServerAddress.sin_port = htons(port);

	// bind the socket to the address and port number
	if (bind(storageServerSocket, (struct sockaddr *)&storageServerAddress, sizeof(storageServerAddress)) < 0)
	{
		perror(red "[-] Bind");
		exit(EXIT_FAILURE);
	}
	printf(green "[+]Binding to listen to incoming server connections successful with port %d\n" white, port);

	// listen for incoming connections from incoming servers
	int allowed_backlog = 20;
	if (listen(storageServerSocket, allowed_backlog) < 0)
	{
		perror(red "[-]Listen");
		exit(EXIT_FAILURE);
	}
	printf(green "[+]Listening successful\n" white);

	char server_sends[MAX_BUFFER_SIZE];
	char server_gets[MAX_BUFFER_SIZE];

	while (1)
	{
		addr_size = sizeof(incomingAddress);

		// Connecting to the incoming server connection
		incomingSocket = accept(storageServerSocket, (struct sockaddr *)&incomingAddress, &addr_size);
		if (incomingSocket < 0)
		{
			perror(red "[-] Accept");
			exit(EXIT_FAILURE);
		}
		printf(green "[+]Accepted new incoming connection\n" white);

		// receive the no of arguments from the incoming connection server
		int no_args;
		if (recv(incomingSocket, &no_args, sizeof(no_args), 0) < 0)
		{
			perror(red "[-] Recv");
			exit(EXIT_FAILURE);
		}
		printf(blue "[+]Received no of arguments: %d\n" white, no_args);

		char ARGS[no_args][PATH_MAX];
		for (int i = 0; i < no_args; i++)
		{
			// receive the arguments from the incoming connection server
			bzero(server_gets, MAX_BUFFER_SIZE);
			if (recv(incomingSocket, server_gets, sizeof(server_gets), 0) < 0)
			{
				perror(red "[-] Recv");
				exit(EXIT_FAILURE);
			}
			printf(blue "[+]Received argument %d: %s\n" white, i + 1, server_gets);
			strcpy(ARGS[i], server_gets);

			// send acknowledgement to the incoming connection server
			bzero(server_sends, MAX_BUFFER_SIZE);
			strcpy(server_sends, "ACK");
			if (send(incomingSocket, server_sends, sizeof(server_sends), 0) < 0)
			{
				perror(red "[-] Send");
				exit(EXIT_FAILURE);
			}
		}

		// receive final done from the incoming connection server
		bzero(server_gets, MAX_BUFFER_SIZE);
		if (recv(incomingSocket, server_gets, sizeof(server_gets), 0) < 0)
		{
			perror(red "[-] Recv");
			exit(EXIT_FAILURE);
		}

		if (strcasecmp(server_gets, "DONE") != 0)
		{
			printf(red "[-] Invalid done packet\n" white);
			exit(EXIT_FAILURE);
		}

		// decide if it was from naming server or client
		if (port == naming_port)
		{
			printf(green "[+]Successfully received instruction from naming server\n" white);
			int result = execCommand(ARGS, no_args, incomingSocket);

			// send acknowledgement to the incoming connection server on the basis of the results of the operation
			bzero(server_sends, MAX_BUFFER_SIZE);
			if (result == 1)
			{
				strcpy(server_sends, "ACK");
				printf(green "[+]Successfully executed naming server command\n" white);
			}
			else
			{
				strcpy(server_sends, "FAIL");
				printf(red "[-] Failed to execute naming server command\n" white);
			}

			if (send(incomingSocket, server_sends, sizeof(server_sends), 0) < 0)
			{
				perror(red "[-] Send");
				exit(EXIT_FAILURE);
			}
		}
		else if (port == client_port)
		{
			printf(green "[+]Successfully received instruction from client\n" white);
			int result = execClientCommand(ARGS, no_args, incomingSocket);

			if (result == 1)
			{
				printf(green "[+]Successfully executed client command\n" white);
			}
			else if (result == 0)
			{
				printf(red "[-] Failed to execute client command\n" white);
			}
			else
			{
				printf(red "[-] Invalid command\n" white);
			}
		}

		// close the socket
		printf(blue "[+]Closed incoming socket\n" white);
		close(incomingSocket);
	}
	return NULL;
}

int main()
{
	path_to_sem = hashmap_create();

	// getting information about the storage server so that it can be sent to the naming server
	StorageServer storageServer;
	printf(green "[+]Starting storage server init process\n" white);

	strcpy(storageServer.ip, ip);

	printf(pink "[+]Enter port for naming server connections: ");
	scanf("%d", &naming_port);
	storageServer.naming_port = naming_port;

	printf(pink "[+]Enter port for client connections: ");
	scanf("%d", &client_port);
	storageServer.client_port = client_port;

	strcpy(storageServer.accessible_paths[0], "/home/");
	// get the files in the home directory

	getcwd(storage_server_path, sizeof(storage_server_path));

	int MAX_FILES = 1024;
	char *files[MAX_FILES];
	for (int i = 0; i < MAX_FILES; i++)
	{
		files[i] = (char *)malloc(MAX_FILE_NAME_LEN * sizeof(char));
	}
	getFiles(storage_server_path, files);

	printf(yellow "[+]Here are the files in the current directory:\n" white);

	for (int i = 0; i < MAX_FILES; i++)
	{
		if (strcasecmp(files[i], "") != 0)
		{
			printf(yellow "\t%d: %s\n" white, i + 1, files[i]);
		}
	}

	printf(pink "[+]Enter the number of files you want to make accessible (you can choose atmost %d) :\n" white, MAX_ACC_PATHS);
	int num_files;
	scanf("%d", &num_files);
	if (num_files > 15)
	{
		printf(red "[-] You can choose atmost %d files\n" white, MAX_ACC_PATHS);
		exit(EXIT_FAILURE);
	}

	it_array = 0;
	associated_files = 0;

	for (int i = 0; i < num_files; i++)
	{
		printf(pink "[+]Enter the number of the file you want to make accessible: ");
		int file_num;
		scanf("%d", &file_num);
		strcpy(storageServer.accessible_paths[it_array], files[file_num - 1]);
		set_value(path_to_sem, files[file_num - 1], associated_files);
		sem_init(&semaphores[associated_files], 0, 1);
		sem_init(&semlocks[associated_files], 0, 1);
		reader_count[associated_files] = 0;
		it_array++;
		associated_files++;
	}

	printf(green "[+]Storage server initialization is complete, connecting to naming server to register\n" white);

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

	char storage_server_gets[MAX_BUFFER_SIZE];
	char storage_server_sends[MAX_BUFFER_SIZE];

	// send the fact that this is a storage server to the naming server
	bzero(storage_server_sends, MAX_BUFFER_SIZE);
	strcpy(storage_server_sends, "s");
	if (send(namingServerSocket, storage_server_sends, sizeof(storage_server_sends), 0) < 0)
	{
		perror(red "[-] Send" white);
		exit(EXIT_FAILURE);
	}

	// receive acknowledgement regarding the start of initiation as a storage server
	bzero(storage_server_gets, MAX_BUFFER_SIZE);
	if (recv(namingServerSocket, storage_server_gets, sizeof(storage_server_gets), 0) < 0)
	{
		perror(red "[-] Recv" white);
		exit(EXIT_FAILURE);
	}

	// send the storage server's information to the naming server
	if (send(namingServerSocket, &storageServer, sizeof(storageServer), 0) < 0)
	{
		perror(red "[-] Send" white);
		exit(EXIT_FAILURE);
	}

	// receive acknowledgement from naming server
	bzero(storage_server_gets, MAX_BUFFER_SIZE);
	if (recv(namingServerSocket, storage_server_gets, sizeof(storage_server_gets), 0) < 0)
	{
		perror(red "[-] Recv" white);
		exit(EXIT_FAILURE);
	}

	printf(blue "[+]Received from naming server: %s\n" white, storage_server_gets);

	// checking if the storage server creation process was successful or not
	if (strcasecmp(storage_server_gets, "ACK") == 0)
	{
		printf(green "[+]Storage server successfully registered with naming server\n" white);
	}
	else
	{
		printf(red "[-] Storage server registration failed. The port mentioned is already in use.\n" white);
		exit(EXIT_FAILURE);
	}

	// close the socket
	close(namingServerSocket);
	printf(blue "[+]Closed naming server socket\n" white);

	// create a thread for listening to naming server connections
	pthread_t namingServerThread;
	int *namingServerPort = malloc(sizeof(int));
	*namingServerPort = naming_port;
	if (pthread_create(&namingServerThread, NULL, portThread, namingServerPort) != 0)
	{
		perror(red "[-] Pthread create");
		exit(EXIT_FAILURE);
	}

	// create a thread for listening to client connections
	pthread_t clientThread;
	int *clientPort = malloc(sizeof(int));
	*clientPort = client_port;
	if (pthread_create(&clientThread, NULL, portThread, clientPort) != 0)
	{
		perror(red "[-] Pthread create");
		exit(EXIT_FAILURE);
	}

	// wait for the threads to finish
	pthread_join(namingServerThread, NULL);
	pthread_join(clientThread, NULL);

	return 0;
}
