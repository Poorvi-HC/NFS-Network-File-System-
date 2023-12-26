#ifndef LRU_H_
#define LRU_H_

#include "../definitions.h"
#include "../hashmap/map.h"

typedef struct Node Node;

struct Node
{
    char *key;
    int val;
    Node *next;
    Node *prev;
};

typedef struct LRU
{
    Node *head;
    Node *tail;
    int capacity;
    hashmap *m;
    int map_size;
} LRU;

void set_value_lr(hashmap *m, char *key, Node *val);
Node *get_value_lr(hashmap *m, char *key);
void remove_entry_lr(hashmap *m, char *key);

LRU *createLRU(hashmap* m, int capacity);
void addNode(LRU *lru, Node *newnode);
void deleteNode(Node *delnode);
int get_LRU(LRU *lru, char *key);
void put_LRU(LRU *lru, char *key, int value);


#endif