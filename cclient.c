/******************************************************************************
* myClient.c
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
#include <ctype.h>

#include "networks.h"
#include "safeUtil.h"
#include "pdu.h" /* added pdu */
#include "pollLib.h" /* added pollLib */

#define MAXBUF 1024
#define DEBUG_FLAG 1

void sendToServer(int socketNum);
int readFromStdin(uint8_t * buffer);
void checkArgs(int argc, char * argv[]);
/* client and poll funx*/
void clientControl(int socketNum); 
void processStdin(int socketNum); 
void processMsgFromServer(int socketNum); 
/* handle functions*/
void registerHandle(int socketNum, char* handle);
void unicast(int socketNum, uint8_t* buffer, int sendLen);
void list(int socketNum);
void broadcast(int socketNum, uint8_t* buffer, int sendLen);
void multicast(int socketNum, uint8_t* buffer, int sendLen);

/* the almighty handle*/
char almightyHandle[100]; /* again the almighty handle for flags and whatnot*/

int main(int argc, char * argv[])
{
	int socketNum = 0;         //socket descriptor
	checkArgs(argc, argv);
	/* set up the TCP Client socket  */
	socketNum = tcpClientSetup(argv[2], argv[3], DEBUG_FLAG);
	/* sendToServer(socketNum); */ /* replaced with client control*/
	strncpy(almightyHandle, argv[1], 100); /* copy handle to the almighty handle */
	almightyHandle[99] = '\0'; /* null terminate the almighty handle */
	registerHandle(socketNum, argv[1]); /* send flag 1 for registration */
	printf("$: ");
	fflush(stdout);
	clientControl(socketNum); /* added */
	close(socketNum);
	return 0;
}

void sendToServer(int socketNum)
{
	uint8_t buffer[MAXBUF];   //data buffer
	int sendLen = 0;        //amount of data to send
	int sent = 0;            //actual amount of data sent /* get the data and send it   */
	int recvBytes = 0;
	
	sendLen = readFromStdin(buffer);
	printf("$: " );
	fflush(stdout);
	
	sent =  sendPDU(socketNum, buffer, sendLen); /* changed to sendPDU */
	if (sent < 0)
	{
		perror("send call");
		exit(-1);
	}

	printf("Socket:%d: Sent, Length: %d msg: %s\n", socketNum, sent, buffer);
	
	// just for debugging, recv a message from the server to prove it works.
	recvBytes = recvPDU(socketNum, buffer, MAXBUF); /* changed to recvPDU */
	printf("Socket %d: Byte recv: %d message: %s\n", socketNum, recvBytes, buffer);
	
}

int readFromStdin(uint8_t * buffer)
{
	char aChar = 0;
	int inputLen = 0;        
	
	// Important you don't input more characters than you have space 
	buffer[0] = '\0';
	printf("$: ");
	fflush(stdout);
	while (inputLen < (MAXBUF - 1) && aChar != '\n')
	{
		aChar = getchar();
		if (aChar != '\n')
		{
			buffer[inputLen] = aChar;
			inputLen++;
		}
	}
	
	// Null terminate the string
	buffer[inputLen] = '\0';
	inputLen++;
	
	return inputLen;
}

void checkArgs(int argc, char * argv[])
{
	/* check command line arguments, updated to 4 to account for the handle*/
	if (argc != 4)
	{
		printf("usage: %s handle host-name port-number\n", argv[0]);
		exit(1);
	}
}
void clientControl(int socketNum)
{
    setupPollSet();
    addToPollSet(socketNum); /* check for socket stuff*/
    addToPollSet(STDIN_FILENO); /* check for stdin stuff*/
    
    while(1)
    {
        int socket = pollCall(POLL_WAIT_FOREVER); /* wait for something to happen */
        if (socket == STDIN_FILENO) /* stdin input */
            processStdin(socketNum); /* data from stdin */
        else
            processMsgFromServer(socketNum); /* data from server */
    }
}
void processStdin(int socketNum)
{
	uint8_t buffer[MAXBUF]; 
	int sendLen = 0;       
	sendLen = readFromStdin(buffer);
	/* newly added*/
	char* token = strtok((char *)buffer, " "); /* split into tokens to be able to get the %m and whatnot and handle and such*/
	if (token[0] == '%'){
		switch(toupper(token[1])){ /* check for m,b,c,l and force all to be upper to account for MLBC*/
			case 'M': /* unicast*/
				unicast(socketNum, buffer, sendLen);
				break;
			case 'C': /* multicast */
				multicast(socketNum, buffer, sendLen); 
				break;
			case 'B': /* broadcast */
				broadcast(socketNum, buffer, sendLen);
				break;
			case 'L': /* list */
				list(socketNum); 
				break;
			default:
				printf("invalid command.\n");
				return;
		}
	}
}
void processMsgFromServer(int socketNum)
{
	uint8_t buffer[MAXBUF];
	int recvBytes = 0;

	recvBytes = recvPDU(socketNum, buffer, MAXBUF); /* receive PDU */

	/* decalre variables after recvPDU*/
	int senderLen = buffer[1]; 
	int destLen = buffer[2 + senderLen + 1];
	int handleLen = buffer[1];
	uint32_t count;
	memcpy(&count, buffer + 1, 4); /* 4 for 4 bytes flag 11*/
	int offset = 2 + senderLen; /* offset to dest len */
	int multiHandle = buffer[offset]; /* num dest for multicast */
	offset++; /* move to dest list */

	if (recvBytes <= 0) /* connetion there or not */
	{
		printf("Server terminated\n");
		close(socketNum);
		exit(0);
	}
	switch (buffer[0]){ /* flag checker*/
	case 4: 
		printf("\n%.*s: %s\n", senderLen, buffer + 2, buffer + 2 + senderLen);
		printf("$: ");
        fflush(stdout);
		break;
	case 5: 
		printf("\n%.*s: %s\n", senderLen, buffer + 2, buffer + 2 + senderLen + 2 + destLen);
		printf("$: ");
        fflush(stdout);
		break;
	case 6: 
		for (int i = 0; i < multiHandle; i++){ /* loop through dest list for multicast and print them out*/
			int destHandleLen = buffer[offset];
			offset++;
			offset += destHandleLen; /* move offset to msg */
		}
			printf("\n%.*s: %s\n", senderLen, buffer + 2, buffer + offset);
			printf("$: ");
			fflush(stdout);
		break;
	case 7: 
		printf("Client with handle %.*s does not exist.\n",handleLen, buffer + 2);
		printf("$: ");
        fflush(stdout);
		break;
	case 11: 
		printf("Number of clients: %d\n", ntohl(count));
		break;
	case 12:
		printf("  %.*s\n", handleLen, buffer + 2);
		break;
	case 13: 
		printf("$: ");
		fflush(stdout);
		break;
	default:
		printf("Server Terminated\n");
	}
}
void registerHandle(int socketNum, char* handle)
{
	uint8_t buffer[MAXBUF];
	int handleLen = strlen(handle);
	buffer[0] = 1; /* flag 1 */
	buffer[1] = handleLen; /* handle len */
	memcpy(buffer + 2, handle, handleLen); /* copy handle to buffer */
	int sendLen = 2 + handleLen; /* total length to send */
	if (sendLen > 100){ /* if bigger than 100 no mas */
		printf("Invalid handle longer than 100 characters: %s\n", handle);
		exit(-1);
	}
	sendPDU(socketNum, buffer, sendLen); /* send PDU */ /* removed int sent due to compiler errors*/
	uint8_t response[MAXBUF];
	recvPDU(socketNum, response, MAXBUF); /* receive response */ /* removed int recvBytes due to compiler errors*/
	if (response[0] == 3) /* flag 3 so fail*/
	{
		printf("Handle already in use: %s\n", handle);
		exit(-1);
	}
	else if (response[0] == 2) /* flag 2 so success */
	{
		printf("Handle registration successful: %s\n", handle);
	}
	else /* unknown response*/
	{
		printf("Unexpected response from server: %d\n", response[0]);
		exit(-1);
	}	
}
void unicast(int socketNUM, uint8_t* buffer, int sendLen){
	uint8_t flagBuf[MAXBUF];
	char* token = strtok(NULL, " "); /* fixed now so not getting %m */
	if (token == NULL){ /* lil error check*/
		printf("Invalid command.\n");
		return;
	}
	char* targetHandle = token; /* where we sending to*/
	token = strtok(NULL, ""); /* add this to get the right msg*/
	char* message;
	if (token == NULL){ /* empty message ppush out anyway*/
		message = "";
	}
	else { /* message like normal*/
		message = token;
	}
	int messageLen = strlen(message);
	int senderHandleLen = strlen(almightyHandle); /* use of the almighty handle*/
	flagBuf[0] = 5;
	flagBuf[1] = senderHandleLen; 
	memcpy(flagBuf + 2, almightyHandle, senderHandleLen); 
	int targetHandleLen = strlen(targetHandle);
	flagBuf[2 + senderHandleLen] = 1; /* always 1 for m*/
	flagBuf[2 + senderHandleLen + 1] = targetHandleLen; 
	memcpy(flagBuf + 2 + senderHandleLen + 2, targetHandle, targetHandleLen); 
	memcpy(flagBuf + 2 + senderHandleLen + 2 + targetHandleLen, message, messageLen + 1); 
	int totalLen = 2 + senderHandleLen + 2 + targetHandleLen + messageLen + 1; 
	sendPDU(socketNUM, flagBuf, totalLen);
}
void list(int socketNum){
	uint8_t flagBuf[1]; 
	flagBuf[0] = 10; 
	sendPDU(socketNum, flagBuf, 1); /* send flag 10 to request list */
}
void broadcast(int socketNum, uint8_t* buffer, int sendLen){
	uint8_t flagBuf[MAXBUF];
	char* token = strtok(NULL, ""); /* get the message */
	char* message;
	if (token == NULL){ 
		message = "";
	}
	else {
		message = token;
	}
	int messageLen = strlen(message);
	int senderHandleLen = strlen(almightyHandle); /* use of the almighty handle*/
	flagBuf[0] = 4;
	flagBuf[1] = senderHandleLen;
	memcpy(flagBuf + 2, almightyHandle, senderHandleLen); 
	memcpy(flagBuf + 2 + senderHandleLen, message, messageLen + 1); 
	int totalLen = 2 + senderHandleLen + messageLen + 1; 
	sendPDU(socketNum, flagBuf, totalLen);
}
void multicast(int socketNum, uint8_t* buffer, int sendLen){
	uint8_t flagBuf[MAXBUF];
	char* token = strtok(NULL, " "); /* get the handle */
	uint8_t multiHandle = atoi(token); /* we will have 2-9 handles so convert to int to check easeir*/
	if (multiHandle < 2 || multiHandle > 9){ /* check if in range of handles */
		printf("oops out of range or error of some sort :(\n");
		return;
	}
	int senderHandleLen = strlen(almightyHandle); /* use of the almighty handle*/
	flagBuf[0] = 6;
	flagBuf[1] = senderHandleLen; 
	memcpy(flagBuf + 2, almightyHandle, senderHandleLen);
	flagBuf[2 + senderHandleLen] = multiHandle;
	int offset = 2 + senderHandleLen + 1; /* current pos*/
	for (int i = 0; i < multiHandle; i++){ /*looping to for all the handles*/
		token = strtok(NULL, " ");
		if (token == NULL){ /* error check in case it breaks in loop*/
			printf("oh no multicast loop error\n");
			return;
		}
		int destHandleLen = strlen(token);
		flagBuf[offset] = destHandleLen; /* apply the dest to offset so its in the right order*/
		offset++;
		memcpy(flagBuf + offset, token, destHandleLen);
		offset += destHandleLen; /* iterate the loop now for offset so we stay in the right place*/
	}
	char* message;
	token = strtok(NULL, ""); /* now to the meat of it, were getting the message */
	if (token == NULL){/* if empty still send basically*/
		message = "";
	}
	else {
		message = token;
	}
	int messageLen = strlen(message);
	memcpy(flagBuf + offset, message, messageLen + 1);
	offset += messageLen + 1; /* update offset*/
	sendPDU(socketNum, flagBuf, offset);	
}