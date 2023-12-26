#ifndef map_operations_h
#define map_operations_h

#include "map.h"
#include "../definitions.h"

typedef struct arr_of_paths {
    char* path;
    hashmap* m;
    char **paths;
    int index;
    int ss_num;
} arr_of_paths;

void set_value(hashmap *m, char *key, int val);
int get_value(hashmap *m, char *key);
void get_paths_(void *key, size_t ksize, uintptr_t value, void *arr);
arr_of_paths* get_paths(hashmap *m, char *path);
void print_map_(void *key, size_t ksize, uintptr_t value, void *arr);
void print_map(hashmap *m);
void remove_entry_(void *key, size_t ksize, uintptr_t value, void *path_struct);
void remove_entry(hashmap *m, char *path);
void add_paths_to_list(hashmap *m, int ss_num, char *current_dir, char *folder_name);

#endif