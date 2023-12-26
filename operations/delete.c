// file contains the implementation of the delete operation
// any string containing a "period" is a file
// COMMAND: DELETE <PATH> <FILE/DIR>

// include external libraries
#include "delete.h"

extern int associated_files;
extern hashmap *path_to_sem;

// retval:
// syntax error: -1, success: 1, fail: 0
int runDelete(char args[][PATH_MAX], int no_args)
{
    // int no_args = 0;
    // for (int i = 0; args[i] != NULL; i++)
    // {
    //     no_args++;
    // }

    if (no_args != 2)
    {
        printf("[-] Invalid number of arguments. COMMAND: DELETE <PATH>\n");
        return -1;
    }

    char path[PATH_MAX];
    strcpy(path, args[1]);

    if (deleteItem(path))
        return 1;
    return 0;
}

int deleteItem(char *path)
{
    // Check if the pathname contains a period
    char *extension = strchr(path, '.');

    // Concatenate the path and filename to create the full path
    char full_path[PATH_MAX];
    snprintf(full_path, sizeof(full_path), ".%s", path);

    // Create an array to hold the command and its arguments
    char *command[4];

    // if the period isn't the first or the last character
    if (extension && extension != path && extension[1] != '\0')
    {
        printf("FILE\n");
        command[0] = "rm";
        command[1] = full_path;
        command[2] = NULL;
        command[3] = NULL;
    }
    else
    {   
        printf("DIR\n");
        command[0] = "rm";
        command[1] = "-rf";
        command[2] = full_path;
        command[3] = NULL;
    }

    // Fork a new process
    pid_t pid = fork();

    if (pid < 0)
    {
        perror("[-] Fork failed");
        return 0; // Deletion failed
    }
    else if (pid == 0)
    {
        execvp(command[0], command);

        perror("[-] Execvp failed");
        exit(EXIT_FAILURE);
    }
    else
    {
        int status;
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0)
        {
            printf("[+]Deleted: %s\n", full_path);
            // remove from the map
            remove_entry(path_to_sem, full_path + 1);
            // need to remove it from the list of accessible paths as well
            return 1;
        }
        else
        {
            printf("[-] Execution failed\n");
            return 0;
        }
    }
}
