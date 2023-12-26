// file contains the implementation of the create operation
// any string containing a "period" is a file
// COMMAND: CREATE <PATH> <FILE/DIR>

// include external libraries
#include "create_file.h"

extern int associated_files;
extern hashmap *path_to_sem;

// retval:
// syntax error: -1, success: 1, fail: 0
int runCreate(char args[][PATH_MAX], int no_args)
{
    if (no_args != 3)
    {
        printf("[-] Invalid number of arguments. COMMAND: CREATE <PATH> <FILENAME>\n");
        return -1;
    }

    char path[PATH_MAX], filename[PATH_MAX];
    strcpy(path, args[1]);
    strcpy(filename, args[2]);

    if (create(path, filename))
        return 1;
    return 0;
}

int create(char *path, char *filename)
{
    // Concatenate the path and filename to create the full path
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), ".%s/%s", path, filename);

    // Check if the filename contains a period
    char *extension = strchr(filename, '.');

    // Create an array to hold the command and its arguments
    char *command[3];

    // if the period isnt the first or the last character
    if (extension && extension != filename && extension[1] != '\0')
    {
        // If the filename contains a period, use the touch command to create a new file
        command[0] = "touch";
        command[1] = full_path;
        command[2] = NULL;
    }
    else
    {
        // If the filename does not contain a period, use the mkdir command to create a new directory
        command[0] = "mkdir";
        command[1] = full_path;
        command[2] = NULL;
    }

    // Fork a new process
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("[-] Fork failed");
        return 0; // Creation failed
    }
    else if (pid == 0)
    {
        // Child process
        execvp(command[0], command);

        // If execvp returns, an error occurred
        perror("[-] Execvp failed");
        exit(EXIT_FAILURE);
    }
    else
    {
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            printf("[+]Created: %s\n", full_path);
            // need to add it to the list of accessible paths as well
            // add it to the hashmap
            set_value(path_to_sem, full_path + 1, associated_files);
            associated_files++;
            return 1;
        }
        else
        {
            printf("[-] Execution failed\n");
            return 0;
        }
    }
}
