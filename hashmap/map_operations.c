#include "map_operations.h"

// Hashes a key-value pair as {key, value} -> {path, SS_ID}
void set_value(hashmap *m, char *key, int val)
{
    char *temp = (char *)malloc(PATH_MAX + 5);
    // printf("size of %s is %d\n", key, strlen(key) - 1);
    sprintf(temp, "%ld %d %s", (size_t)strlen(key) - 1, key[strlen(key) - 1], key);
    // printf("key:%s value:%d\n",temp,val);
    hashmap_set(m, temp, strlen(temp) - 1, val);
    return;
    free(temp);
}

// Returns the value associated with a key if found, else -1
int get_value(hashmap *m, char *key)
{
    uintptr_t result;
    char *temp = (char *)malloc(PATH_MAX + 5);
    sprintf(temp, "%ld %d %s", (size_t)strlen(key) - 1, key[strlen(key) - 1], key);
    if (hashmap_get(m, temp, strlen(temp) - 1, &result))
    {
        return (int)result;
    }
    else
    {
        printf("error: unable to locate entry \"%s\"\n", key);
        return -1;
    }
    free(temp);
}

// Iterative calling of this function
void get_paths_(void *key, size_t ksize, uintptr_t value, void *arr)
{
    arr_of_paths *path_struct = (arr_of_paths *)arr;
    char *temp = (char *)malloc(PATH_MAX + 5);
    int _;
    sscanf(key, "%d %d %s", &_, &_, temp);

    if (strncmp(temp, path_struct->path, strlen(path_struct->path)) == 0)
    {
        if (temp[strlen(path_struct->path)] == '\0' || temp[strlen(path_struct->path)] == '/')
        {
            strcpy(path_struct->paths[path_struct->index], temp);
            path_struct->index++;
        }
    }
    free(temp);
}

arr_of_paths *get_paths(hashmap *m, char *path)
{
    arr_of_paths *arr = (arr_of_paths *)malloc(sizeof(arr_of_paths));
    arr->path = path;
    arr->paths = (char **)malloc(sizeof(char *) * MAX_ACC_PATHS);
    for (int i = 0; i < MAX_ACC_PATHS; i++)
    {
        arr->paths[i] = (char *)malloc(sizeof(char) * PATH_MAX);
    }
    arr->index = 0;
    arr->ss_num = -1;
    arr->m = m;
    hashmap_iterate(m, get_paths_, (void *)arr);
    return arr;
}

void print_map_(void *key, size_t ksize, uintptr_t value, void *arr)
{
    char* temp = (char *)malloc(PATH_MAX + 5);
    int _;
    sscanf(key, "%d %d %s", &_, &_, temp);
    printf("[+]Key: %s , Value: %d\n", (char *)temp, (int)value);
    free(temp);
}

void remove_entry_(void *key, size_t ksize, uintptr_t value, void *path_struct)
{
    arr_of_paths *arr = (arr_of_paths *)path_struct;
    char *temp = (char *)malloc(PATH_MAX + 5);
    int _;
    sscanf(key, "%d %d %s", &_, &_, temp);

    if (strncmp(temp, arr->path, strlen(arr->path)) == 0)
    {
        if (temp[strlen(arr->path)] == '\0' || temp[strlen(arr->path)] == '/')
            hashmap_remove(arr->m, key, ksize);
    }
    free(temp);
}

void remove_entry(hashmap *m, char *path)
{
    arr_of_paths *arr = (arr_of_paths *)malloc(sizeof(arr_of_paths));
    arr->path = path;
    arr->paths = NULL;
    arr->index = 0;
    arr->ss_num = -1;
    arr->m = m;
    hashmap_iterate(m, remove_entry_, (void *)arr);
    free(arr);
}

// Prints the complete map
void print_map(hashmap *m)
{
    hashmap_iterate(m, print_map_, NULL);
}

void add_paths_to_list(hashmap *m, int ss_num, char *current_dir, char *folder_name)
{
    arr_of_paths *arr = get_paths(m, folder_name);
    int i = strlen(folder_name) - 1;
    while(folder_name[i] != '/') i--;

    char *temp = (char *)malloc(PATH_MAX + 5);
    for (int i = 0; i < arr->index; i++)
    {
        sprintf(temp, "%s/%s", current_dir, arr->paths[i] + i + 1);
        set_value(m, temp, arr->ss_num);
    }
    free(temp);
    free(arr);
}