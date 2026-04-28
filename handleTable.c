#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>

#include "handleTable.h"
#include "pdu.h"

static struct handleEntry* head = NULL;

int add(char* handleName, int handleLen, int socketNum){ /* lets add a handle*/
    if (lookupByHandleName(handleName) != -1){ /*check if handle exists*/
        return 0; /* handle exits already */
    }
    struct handleEntry* newEntry = malloc(sizeof(struct handleEntry)); /* new entry time */
    strncpy(newEntry->handleName, handleName, 100); /* get that handle name, aka copy it */
    newEntry->handleName[99] = '\0'; /* make sure there is null termination */
    newEntry->handleLen = handleLen; 
    newEntry->socketNum = socketNum; 
    newEntry->next = NULL; /* new entry end of list*/

    if (head == NULL){ /* if list empty*/
        head = newEntry;
        return 1;
    }
    struct handleEntry* curr = head; /* end of lsit time*/
    while (curr->next != NULL){
        curr = curr->next; 
    }
    curr->next = newEntry;
    return 1;
}

int lookupByHandleName(char* handleName){/* looky lookyy handle*/
    struct handleEntry* curr = head;
    while (curr != NULL){/* check each handle name */
        if (strcmp(curr->handleName, handleName) ==0 ){ /* compare if the handles match*/
            return curr->socketNum;
        }
        curr = curr->next;
    }
    return -1; /* oops didnt find no handle */
}

char* lookupBySocket(int socketNum){ /* socket where you at*/
    struct handleEntry* curr = head;
    while (curr != NULL){ /* check each socket num */
        if (curr->socketNum == socketNum){ /* if match then return */
            return curr->handleName;
        }
        curr = curr->next;
    }
    return NULL; /* else that heckin null */
}

void removeBySocket(int socketNum){ /* socket removal*/
    struct handleEntry* curr = head;/* where were lookin*/
    struct handleEntry* prev = NULL; /* prev */
    while (curr != NULL){ /* loop for socket*/
        if (curr->socketNum == socketNum){ /* if socket fount*/
            if (prev == NULL){ /* if head*/
                head = curr->next; /* head update*/
            } else {
                prev->next = curr->next; /* else its not head*/
            }
            free(curr); /* free mem*/
            return;
        }
        prev = curr; /* update prev and move curr*/
        curr = curr->next;
    }
}

void sendListToSocket(int socketNum){
    struct handleEntry* curr = head;
    uint8_t flagBuf[102]; /* flag buffer for sendPDU thats big enough for flags11-13 to use*/
    int count = 0; /* count for the handles*/
    while (curr != NULL){ /* loop list first time for flag 11*/
        count++;
        curr = curr->next;
    }
    flagBuf[0] = 11; /* flag 11 for number of handles on server*/
    uint32_t nCount = htonl(count); /* convert count to netwrok so we have the 4 bytes needed*/
    memcpy(flagBuf + 1, &nCount, 4); /* get flag then count into buff*/
    sendPDU(socketNum, flagBuf, 5); /* buff send yep time yep*/
    curr = head; /* forgot initially but need to reset the head for the second loop*/
    while (curr != NULL){ /* second loop for flag 12*/
        /* flag buffer for sendPDU which needs up to 100 bytes plsu flag and len*/
        flagBuf[0] = 12; /* one flag 12 packet per handle*/
        flagBuf[1]  = curr->handleLen; /* get the current handlelen and put that in the buffer as well*/
        memcpy(flagBuf + 2, curr->handleName, curr->handleLen); /* get the handlename and length into full buffer prb*/
        sendPDU(socketNum, flagBuf, 2 + curr->handleLen); /* now we send the 12 flag one for each handle*/
        curr = curr->next;
    }
    /* now we use the buffer for flag 13 to be sent*/
    flagBuf[0] = 13; /* flag 13 to indicate we are done*/
    sendPDU(socketNum, flagBuf, 1); /* wow were sending*/
}
void traverseList(int socketNum, uint8_t* dataBuffer, int messageLen){ /* this is for flag 4 so broadcast*/
    struct handleEntry* curr = head;
    while (curr != NULL){ /* loop list */
        if (curr->socketNum != socketNum){ /* if not sender then send*/
            sendPDU(curr->socketNum, dataBuffer, messageLen); /* send to each client except sender*/
        }
        curr = curr->next;
    }
}