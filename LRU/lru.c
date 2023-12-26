#include "lru.h"

// Hashes a key-value pair as {key, value} -> {path, Node* }
void set_value_lr(hashmap *m, char *key, Node *val)
{
    char *temp = (char *)malloc(PATH_MAX + 5);
    sprintf(temp, "%ld %d %s", strlen(key), key[strlen(key) - 1], key);
    hashmap_set(m, temp, strlen(temp), (uintptr_t)val);
    return;
}

// Returns the value associated with /operations/c/a.txt key if found, else -1
Node *get_value_lr(hashmap *m, char *key)
{
    uintptr_t result;

    char *temp = (char *)malloc(PATH_MAX + 5);
    sprintf(temp, "%ld %d %s", strlen(key), key[strlen(key) - 1], key);

    if (hashmap_get(m, temp, strlen(temp), &result))
    {
        return (Node *)result;
    }
    else
    {
        return NULL;
    }
}

void remove_entry_lr(hashmap *m, char *key)
{
    char *temp = (char *)malloc(PATH_MAX + 5);
    sprintf(temp, "%ld %d %s", strlen(key), key[strlen(key) - 1], key);
    hashmap_remove(m, temp, strlen(temp));
}

// creates a new LRU with the given capacity
LRU *createLRU(hashmap *m, int capacity)
{
    LRU *lru = (LRU *)malloc(sizeof(LRU));
    lru->head = (Node *)malloc(sizeof(Node));
    lru->tail = (Node *)malloc(sizeof(Node));
    lru->head->key = "#";
    lru->head->val = -1;
    lru->tail->key = "#";
    lru->tail->val = -1;
    lru->head->next = lru->tail;
    lru->tail->prev = lru->head;

    lru->capacity = capacity;
    lru->m = m;
    lru->map_size = 0;

    return lru;
}

void addNode(LRU *lru, Node *newnode)
{
    Node *headnext = lru->head->next;
    lru->head->next = newnode;
    newnode->prev = lru->head;
    newnode->next = headnext;
    headnext->prev = newnode;
}

void deleteNode(Node *delnode)
{
    Node *prevv = delnode->prev;
    Node *nextt = delnode->next;

    prevv->next = nextt;
    nextt->prev = prevv;
}

// returns the value (int) by the given key (char* )
int get_LRU(LRU *lru, char *key)
{
    Node *resNode;

    if ((resNode = get_value_lr(lru->m, key)) != NULL)
    {
        int ans = resNode->val;
        remove_entry_lr(lru->m, key);
        lru->map_size--;
        deleteNode(resNode);
        addNode(lru, resNode);
        set_value_lr(lru->m, key, lru->head->next);
        lru->map_size++;
        return ans;
    }
    return -1;
}

// put values in the lru as {char* ,int}
void put_LRU(LRU *lru, char *key, int value)
{
    Node *curr;

    if ((curr = get_value_lr(lru->m, key)) != NULL)
    {
        remove_entry_lr(lru->m, key);
        lru->map_size--;
        deleteNode(curr);
    }

    if (lru->map_size == lru->capacity)
    {
        remove_entry_lr(lru->m, lru->tail->prev->key);
        deleteNode(lru->tail->prev);
        lru->map_size--;
    }

    Node *newnode = (Node *)malloc(sizeof(Node));
    newnode->key = key;
    newnode->val = value;

    addNode(lru, newnode);

    set_value_lr(lru->m, key, lru->head->next);
    lru->map_size++;
}
