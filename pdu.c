#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "safeUtil.h"
#include "pdu.h"

int sendPDU(int clientSocket, uint8_t * dataBuffer, int lengthOfData){
    int totalPDUlen = lengthOfData + 2; /* 2 bytes for head*/
    uint8_t pduBUF[totalPDUlen];
    uint16_t netTotalPDUlen = htons(totalPDUlen); /* total lenght in network */
    memcpy(pduBUF, &netTotalPDUlen, 2);
    memcpy(pduBUF + 2, dataBuffer, lengthOfData);
    safeSend(clientSocket, pduBUF, totalPDUlen, 0);      
    return lengthOfData;
}
int recvPDU(int socketNumber, uint8_t * dataBuffer, int bufferSize){
    uint16_t totalPDUlen;
    if (safeRecv(socketNumber, (uint8_t *)&totalPDUlen, 2, MSG_WAITALL) == 0){ /* check if connection there */
        return 0;
    }
    totalPDUlen = ntohs(totalPDUlen); /* network to host*/
    int Datalen = totalPDUlen - 2;
    if (Datalen > bufferSize){ /* is buff big?? */
        fprintf(stderr, "buffer is too small\n");
        exit(-1);
    }
    safeRecv(socketNumber, dataBuffer, Datalen, MSG_WAITALL); /* get the data*/
    return Datalen;
}