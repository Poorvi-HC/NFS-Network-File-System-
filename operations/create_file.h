#ifndef CREATE_H
#define CREATE_H

// include external libraries
#include "../definitions.h"
#include "../hashmap/map_operations.h"

// function declarations
int create(char* path, char* name);
int runCreate(char args[][PATH_MAX], int no_args);

#endif