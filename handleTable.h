#ifndef HANDLETABLE_H
#define HANDLETABLE_H

#include <stdint.h>

struct handleEntry{
    char handleName[100];
    int handleLen;
    int socketNum;
    struct handleEntry* next;
};

int add(char* handleName, int handleLen, int socketNum);
int lookupByHandleName(char* handleName);
char* lookupBySocket(int socketNum);
void removeBySocket(int socketNum);
void sendListToSocket(int socketNum);
void traverseList(int socketNum, uint8_t* dataBuffer, int messageLen);
#endif