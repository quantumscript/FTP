/********************************************************************
Class: CS 372 - Intro To Networking
Program: Project 2 FTP with sockets - ftserver.c
Author: KC
Date: August 10 2018
Description: Establish FTP protocol with client through control socket. Send
file or directory list through a data socket.
Sources Used: CS 344 - Operating Systems - Prog 4 boilerplate code on sockets
**********************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>

void listFiles(int, int);
void sendFile(int, int, char []);
int startUp(char*);
int setUpControlSocket(int);
int handleRequest(int);

#define MAX_SIZE 10


/******************************  main() function  ******************************
 ******************************************************************************/
int main(int argc, char *argv[]) {

	// Validate input, check usage and args
	if (argc != 2) { fprintf(stderr,"USAGE: %s port\n", argv[0]); exit(0); }

	// Set up socket
	int listenSocket = startUp(argv[1]);
	while (1) {
		int controlSocket = setUpControlSocket(listenSocket);

		// Get command from the client
		int command = 0;
		char readBuffer[1000];
		memset(readBuffer, '\0', sizeof(readBuffer));
		recv(controlSocket, readBuffer, 1, 0);
		printf("	The command received is: %s\n", readBuffer);
		if (strncmp(readBuffer, "l", 1) == 0) command = 1;		// List command
		else if (strncmp(readBuffer, "g", 1) == 0) command = 2;		// Get file command

		// Send back command, will be interpreted as error
		send(controlSocket, readBuffer, 1, 0);

		// If received valid command
		if (command != 0) {

			// Get dataport from the client
			int dataPort = -1;
			memset(readBuffer, '\0', sizeof(readBuffer));
			recv(controlSocket, readBuffer, 5, 0);
			dataPort = atoi(readBuffer);
			printf("	The port received is: %d\n", dataPort);
			// Send back command as a way to break up the client messages being sent
			send(controlSocket, readBuffer, sizeof(readBuffer), 0);

			// If command is 'g' to get a file, get filename
			int nameSize = 40;
			char fileNameBuff[nameSize];
			memset(fileNameBuff, '\0', sizeof(fileNameBuff));
			if (command == 2) {
				// Get filename from the client
				recv(controlSocket, fileNameBuff, sizeof(fileNameBuff), 0);
				printf("	The filename received is: %s\n", fileNameBuff);
			}

			// Handle request by establishing connection to client's data port
			int dataSocket = handleRequest(dataPort);

			// If command was to list files, send directory contents into data socket
			if (command == 1) listFiles(dataSocket, controlSocket);

			// If command was to send a file, validate filename and send file
			if (command == 2) sendFile(controlSocket, dataSocket, fileNameBuff);

			// Close dataSocket after request is handled
			close(dataSocket);
		}

		// Close connections
		close(controlSocket);
	}

	// Close the FTP server
	close(listenSocket);
	return 0;
}


/******************************  listFiles function  ***************************
	Description: Sends files in directory to client on data socket
 ******************************************************************************/
void listFiles(int dataSocket, int controlSocket) {
	DIR* dirToCheck; // Pointer to active directory once it's open
	dirToCheck = opendir(".");
	char contentName[40];
	memset(contentName, '\0', sizeof(contentName));

	// If opendir is successful
	if (dirToCheck > 0) {
		struct dirent* fileInDir;

		// While the directory stream is not null (while reading)
		while ((fileInDir = readdir(dirToCheck)) != NULL) {
			memset(contentName, '\0', sizeof(contentName));
			strcpy(contentName, fileInDir->d_name);	// Copy new content (file/dir) name
			contentName[strlen(contentName)] = '\n'; // Terminate string with \n
			send(dataSocket, contentName, strlen(contentName), 0);
		}

		// Send termination signal on control socket
		send(controlSocket, "#$!*", 4, 0);
	}
	printf("	Directory contents sent.\n");
	closedir(dirToCheck);
}

/******************************  sendFile function  ****************************
	Description: Reads file and sends to client over data socket
 ******************************************************************************/
void sendFile(int controlSocket, int dataSocket, char fileNameBuff[]) {
	// Open file. If the file does not exist send error message on control socket
	int fileHandle = open(fileNameBuff, O_RDONLY);
	if (fileHandle < 0) send(controlSocket, "File not found\n", 15, 0);

	// Otherwise send confirmation on control socket and the file on the data socket
	else {

		// Read file into buffer, get bytesReadFile, close file descriptor
		char buffFile[MAX_SIZE];
		memset(buffFile, '\0', sizeof(buffFile));

		// Send confirmation on control socket
		send(controlSocket, "ok", 2, 0);
		int bytesReadFile = -1;

		// Read from file and send to data socket
		do {
			bytesReadFile = read(fileHandle, buffFile, MAX_SIZE);
			if (bytesReadFile != 0) {
				send(dataSocket, buffFile, bytesReadFile, 0);
				memset(buffFile, '\0', sizeof(buffFile));
			}
		} while (bytesReadFile != 0);

		// Send termination signal
		send(controlSocket, "#$!*", 4, 0);

		close(fileHandle);
		printf("File successfully sent.\n");
	}

	return;
}


/**************************  Set up socket function  ***************************
	Description: Sets up listen socket for FTP with clients
 ******************************************************************************/
int startUp(char* portNumStr) {

	// Initialize variables for setting up server socket
	int listenSocket, portNumber;
	struct sockaddr_in serverAddress;

	// Set up the address struct for the server
	memset((char *)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
	portNumber = atoi(portNumStr); // Get the port number, convert to an integer from a string
	serverAddress.sin_family = AF_INET; // Create a network-capable socket
	serverAddress.sin_port = htons(portNumber); // Convert to network byte order (big endian)
	serverAddress.sin_addr.s_addr = INADDR_ANY; // Any address is allowed for connection to this process

	// Set up the socket
	listenSocket = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
	if (listenSocket < 0) perror("SERVER: ERROR opening socket");

	// Enable the socket to begin listening
	if (bind(listenSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to port
		perror("SERVER: ERROR on binding");
	listen(listenSocket, 5); // Flip the socket on - it can now receive up to 5 connections

	return listenSocket;
}


/**************************  Set up control socket function  *******************
	Description: Listen socket accepts control socket with client, establish
	communication by receiving and sending "hello"
 ******************************************************************************/
int setUpControlSocket(int listenSocket) {
	// Accept a connection, blocking if one is not available until one connects
	struct sockaddr_in clientAddress;
	socklen_t sizeOfClientInfo = sizeof(clientAddress); // Get the size of the address for the client that will connect
	int controlSocket = accept(listenSocket, (struct sockaddr *)&clientAddress, &sizeOfClientInfo); // Accept
	if (controlSocket < 0) perror("SERVER: ERROR on accept");

	// Receive message from client into buffer
	int msgLen = 5;
	char msgBuff[msgLen];
	strcpy(msgBuff, "hello");
	char readBuffer[msgLen + 1]; // Read buffer to recv() data in small chunks
	memset(readBuffer, '\0', sizeof(readBuffer));
	recv(controlSocket, readBuffer, msgLen, 0);

	// If receive "hello" from client, send "hello" back
	if (strncmp(readBuffer, msgBuff, msgLen) == 0) {
		send(controlSocket, msgBuff, msgLen, 0);
		printf("Control connection at server established\n");
	}
	else {
		close(controlSocket);
		printf("Control connection at server failed\n");
	}

	return controlSocket;
}


/**************************  Handle request function  **************************
	Description: Returns a data socket to transfer information over
 ******************************************************************************/
int handleRequest(int dataPort) {
	// Initialize variables for connecting socket to server
	char host[30];
	memset(host, '\0', sizeof(host));
	gethostname(host, sizeof(host));
	int dataSocket;
	struct sockaddr_in dataAddress;
	struct hostent* dataHostInfo;

	// Set up the data address struct
	memset((char*)&dataAddress, '\0', sizeof(dataAddress)); // Clear out the address struct
	dataAddress.sin_family = AF_INET; // Create a network-capable socket
	dataAddress.sin_port = htons(dataPort); // Store the port number
	dataHostInfo = gethostbyname(host); // Convert the machine name into a special form of address
	if (dataHostInfo == NULL)	perror("CLIENT: ERROR, no such host");
	memcpy((char*)&dataAddress.sin_addr.s_addr, (char*)dataHostInfo->h_addr, dataHostInfo->h_length); // Copy in the address

	// Set up the socket
	dataSocket = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
	if (dataSocket < 0) perror("CLIENT: ERROR opening socket");

	// Connect to client data socket
	if (connect(dataSocket, (struct sockaddr*)&dataAddress, sizeof(dataAddress)) < 0) // Connect socket to address
		perror("CLIENT: ERROR connecting");

	return dataSocket;
}
