/******************************************************************************
* myServer.c
* 
* Writen by Prof. Smith, updated Jan 2023
* Use at your own risk.  
*
*****************************************************************************/

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdint.h>

#include "networks.h"
#include "safeUtil.h"
#include "pdu.h" /* added pdu */
#include "pollLib.h" /* added pollLib */
#include "handleTable.h" /* added handle table */

#define MAXBUF 1024
#define DEBUG_FLAG 1

void recvFromClient(int clientSocket);
int checkArgs(int argc, char *argv[]);
/* server control and poll*/
void serverControl(int mainServerSocket);
void addNewSocket(int mainServerSocket); 
void processClient(int clientSocket); 
/* flag functions*/
void processFlag1(int clientSocket, uint8_t* dataBuffer); 
void processFlag10(int clientSocket); 
void processFlag5(int clientSocket, uint8_t* dataBuffer, int messageLen);
void processFlag6(int clientSocket, uint8_t* dataBuffer, int messageLen);
void processFlag4(int clientSocket, uint8_t* dataBuffer, int messageLen);



int main(int argc, char *argv[])
{
	int mainServerSocket = 0;   //socket descriptor for the server socket
	/* int clientSocket = 0;   //socket descriptor for the client socket (line not used anymore) */
	int portNumber = 0;
	
	portNumber = checkArgs(argc, argv);
	
	//create the server socket
	mainServerSocket = tcpServerSetup(portNumber);

	serverControl(mainServerSocket); /* loop to unitl ^c */
	/* old loop code for the ^c server stuff without polling
	while(1)
	{
	// wait for client to connect
	clientSocket = tcpAccept(mainServerSocket, DEBUG_FLAG);

	
	recvFromClient(clientSocket);
		
	 close the sockets
	close(clientSocket); 
}	*/
	close(mainServerSocket);

	
	return 0;
}

void recvFromClient(int clientSocket) /* changed function to loop until ^c*/
{
    uint8_t dataBuffer[MAXBUF];
    int messageLen = 0;
    
    while ((messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF)) > 0) /* changed to recvPDU */
    {
        printf("Socket %d: Message received, length: %d Data: %s\n", clientSocket, messageLen, dataBuffer);
        sendPDU(clientSocket, dataBuffer, messageLen);
        printf("Socket %d: msg sent: %d bytes, text: %s\n", clientSocket, messageLen, dataBuffer);
    }
    printf("Socket %d: Connection closed\n", clientSocket);
}



int checkArgs(int argc, char *argv[])
{
	// Checks args and returns port number
	int portNumber = 0;

	if (argc > 2)
	{
		fprintf(stderr, "Usage %s [optional port number]\n", argv[0]);
		exit(-1);
	}
	
	if (argc == 2)
	{
		portNumber = atoi(argv[1]);
	}
	
	return portNumber;
}
void serverControl(int mainServerSocket)
{
	setupPollSet();
	addToPollSet(mainServerSocket); /* check main socket for new client*/

	while (1)
	{
		int socket = pollCall(POLL_WAIT_FOREVER); /* wait for something to happen */
		if (socket == mainServerSocket) /* new client */
			addNewSocket(mainServerSocket); /* new client incoming*/
		else
			processClient(socket); /* data from client */
	}
}
void addNewSocket(int mainServerSocket)
{
	int clientSocket = tcpAccept(mainServerSocket, DEBUG_FLAG);
	addToPollSet(clientSocket); /* add new to poll set */
}
void processClient(int clientSocket)
{	uint8_t dataBuffer[MAXBUF];
	int messageLen = 0;
	if ((messageLen = recvPDU(clientSocket, dataBuffer, MAXBUF)) > 0) /* check if message there*/
	{
		switch (dataBuffer[0]){ /* check flags then process from there */
			case 1: /* flag 1 for handle registration */
				processFlag1(clientSocket, dataBuffer);
				break;
			case 4: /* flag 4 for broadcast */
				processFlag4(clientSocket, dataBuffer, messageLen);
				break;
			case 5: /* flag 5 for unicast */
				processFlag5(clientSocket, dataBuffer, messageLen);
				break;
			case 6: /* flag 6 for multicast */
				processFlag6(clientSocket, dataBuffer, messageLen);
				break;
			case 10: /* flag 10 for list request */
				processFlag10(clientSocket);
				break;
			default:
				printf("Socket %d: Invalid flag received: %d\n", clientSocket, dataBuffer[0]);
		}
		/*
		printf("Socket %d: Message received, length: %d Data: %s\n", clientSocket, messageLen, dataBuffer);
		sendPDU(clientSocket, dataBuffer, messageLen);
		printf("Socket %d: msg sent: %d bytes, text: %s\n", clientSocket, messageLen, dataBuffer);
		*/
	}
	else /* else close */
	{
		printf("Socket %d: Connection closed\n", clientSocket);
		removeBySocket(clientSocket); /* remove from handle table */
		removeFromPollSet(clientSocket); /* remove from poll set */
		close(clientSocket); /* close the socket */
	}
}
/* process flag functions are all for unicast broadcast and whatnot byt made more sense to name them
   after what flag is being processed since it made more sense to me.*/
void processFlag1(int clientSocket, uint8_t* dataBuffer){
	char tempHandle[100]; /* temp handle for length */
	int handleLen = dataBuffer[1]; /* get handle len from buffer*/
	memcpy(tempHandle, dataBuffer + 2, handleLen); /* put length in temp */
	tempHandle[handleLen] = '\0'; /* null terminate that handle */
	if (add(tempHandle, handleLen, clientSocket) == 0){ /* add to table if 0 fla3 else flag2*/
		uint8_t flagBuf[1]; /* buff flag 3*/
		flagBuf[0] = 3; /* flag 3 */
		sendPDU(clientSocket, flagBuf, 1); /* fail */
	}
	else {
		uint8_t flagBuf[1]; /* buff flag 2 */
		flagBuf[0] = 2; /* flag 2 */
		sendPDU(clientSocket, flagBuf, 1); /* success */
	}
}
void processFlag10(int clientSocket){
	sendListToSocket(clientSocket); /* send list to client */
}
void processFlag5(int clientSocket, uint8_t* dataBuffer, int messageLen){
	int senderLen = dataBuffer[1]; /* get sender len */   
	int destLen = dataBuffer[2 + senderLen + 1]; /* get dest len, which is the number of dest plus handle len */
	char tempDest[100]; /* temp dest for length */
	memcpy(tempDest, dataBuffer + 2 + senderLen + 2, destLen);	/* put dest in temp */
	tempDest[destLen] = '\0'; /* null term */
	int destSocket = lookupByHandleName(tempDest); /* get dest socket */
	if (destSocket == -1){ /* if not found send falg7 */
		uint8_t flagBuf[102]; /* have to make sure big enough for dest len*/	
		flagBuf[0] = 7; /* flag 7 */
		flagBuf[1] = destLen; /* dest len */
		memcpy(flagBuf + 2, tempDest, destLen); /* dest name */
		sendPDU(clientSocket, flagBuf, 2 + destLen); /* you failed */
		return;
	}else{ /* else send the packet*/
		sendPDU(destSocket, dataBuffer, messageLen); /* forward packet */
	}
}

void processFlag6(int clientSocket, uint8_t* dataBuffer, int messageLen){
	int senderLen = dataBuffer[1];
	int offset = 2 + senderLen; /* offset to dest len */
	int numDest = dataBuffer[offset]; /* get num dest */
	char tempDest[100]; /* temp dest */
	offset++; /* move to dest list */
	for(int i = 0; i < numDest; i++){ /* loop until you get throuhg all dests*/
		int destLen = dataBuffer[offset];
		offset++; /* move to dest name */
		memcpy(tempDest, dataBuffer + offset, destLen); /* put dest in temp */
		tempDest[destLen] = '\0'; /* null term */
		offset += destLen; /* move to next dest len */
		int destSocket = lookupByHandleName(tempDest); /* get dest socket */
		if (destSocket == -1){ /* if not found send flag 7 */
			uint8_t flagBuf[102];	
			flagBuf[0] = 7;
			flagBuf[1] = destLen;
			memcpy(flagBuf + 2, tempDest, destLen); /* dest name */
			sendPDU(clientSocket, flagBuf, 2 + destLen); /* you failed */
		}else{ /* else send the packet*/
			sendPDU(destSocket, dataBuffer, messageLen); /* forward packet */	
		}
	}
}
void processFlag4(int clientSocket, uint8_t* dataBuffer, int messageLen){
	traverseList(clientSocket, dataBuffer, messageLen); /* okay we be broadcastin ig */
}