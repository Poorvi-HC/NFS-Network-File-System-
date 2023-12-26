#ifndef DELETE_H
#define DELETE_H

// include external libraries
#include "../definitions.h"
#include "../hashmap/map_operations.h"

// function declarations
int deleteItem(char* path);
int runDelete(char args[][PATH_MAX], int no_args);

#endif