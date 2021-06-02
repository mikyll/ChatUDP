/*
*	
*	This software is provided 'as-is', without any express or implied
*	warranty. In no event will the authors be held liable for any damages
*	arising from the use of this software.
*	Permission is granted to anyone to use this software for any purpose,
*	including commercial applications, and to alter it and redistribute it
*	freely.
*	
*	-------------------------------------------------------------------------------------------------------
*	
*	USAGE:
*	./chat [test] [1|2]
*	./chat 		      -	normal execution: you specify the IP and port of the destination endpoint
*				you want to send messages to, and the port on which you listen for messages.
*
*	./chat test	      -	test executable(1): IP is set to loopback (127.0.0.1) and both ports are set to
*				4000 so you will send and receive message on the same process, inside the same
*				run.
*
*	./chat test 1|2     -	test executables(2): IP is set to loopback (127.0.0.1) and the ports are set to
*				4001 or 4002, based on the second parameter, so it's possible to test two
*				instances on two different terminal.
*	./chat test 1	      -	runs the endpoint A (listen on 4001, send to 127.0.0.1:4002).
*	./chat test 2	      -	runs the endpoint B (listen on 4002, send to 127.0.0.1:4001).
*				
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>



#define MESSAGE_SIZE 256

typedef struct {
	int day;
	int month;
	int year;
	int hour;
	int minute;
	int second;
} DateTime;
typedef struct {
	DateTime timestamp;
	char message[MESSAGE_SIZE];
} DatagramPacket;

void printFullPacket(DatagramPacket packet, char* senderAddress);
void printChatMessage(DatagramPacket packet, char* senderAddress);

int main(int argc, char ** argv)
{
	int pid, sd, n, len, i;
	char address[16];
	int destPort, listenPort;
	
	struct hostent *host;
	DatagramPacket packet;
	
	time_t rawtime;
	struct tm* timeinfo;
	
	if(argc >= 2 && strcmp(argv[1], "test") == 0)
	{
		strcpy(address, "127.0.0.1");
		if(argc == 2)
		{
			destPort = 4000;
			listenPort = 4000;
		}
		if(argc == 3)
		{
			if(atoi(argv[2]) == 1)
				destPort = 4001, listenPort = 4002;
			else destPort = 4002, listenPort = 4001;
		}
		printf("Destination IP address: %s\nDestination port: %d\nListen port: %d\n", address, destPort, listenPort);
	}
	else
	{
		for(host = NULL; host == NULL;)
		{
			printf("Destination IP address: ");
			scanf("%s", address);
			host = gethostbyname(address);
			if (host == NULL)
				printf("Invalid address (%s). h_errno=%d", address, h_errno);
		}
		for(destPort = 0; destPort < 1024 || destPort > 65535;)
		{
			printf("Destination port: ");
			scanf("%d", &destPort);
			if(destPort < 1024 || destPort > 65535)
				printf("Destination port must be between 1024 and 65535.\n");
		}
		for(listenPort = 0; listenPort < 1024 || listenPort > 65535 || listenPort == destPort;)
		{
			printf("Listen port: ");
			scanf("%d", &listenPort);
			if(listenPort < 1024 || listenPort > 65535 || listenPort == destPort)
				printf("Listen port must be greater than 1024 and different from destination port.\n");
		}
	}
	
	pid = fork();
	// Father process (Client)
	if(pid > 0)
	{
		struct sockaddr_in clientAddr, serverAddr;
		
		// Client and server address initialization
		memset((char *)&clientAddr, 0, sizeof(struct sockaddr_in));
		clientAddr.sin_family = AF_INET;
		clientAddr.sin_addr.s_addr = INADDR_ANY;
		clientAddr.sin_port = 0;
	
		memset((char*)&serverAddr, 0, sizeof(struct sockaddr_in));
		serverAddr.sin_family = AF_INET;
		host = gethostbyname(address);
		
		serverAddr.sin_addr.s_addr=((struct in_addr *)(host->h_addr))->s_addr;
		serverAddr.sin_port = htons(destPort);
	
		// Socket creation
		if((sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			perror("An error occurred while creating the socket "); 
			exit(1);
		}
		printf("Client (%d): socket created (sd=%d)\n", getpid(), sd);
		
		// Binding of the socket(sd) to the network address
		if(connect(sd, (struct sockaddr *) &serverAddr, sizeof(struct sockaddr)) < 0) {
			perror("An error occurred while during the socket binding ");
			exit(1);
		}
		printf("Client (%d): socket successfully connected to %s:%d\n", getpid(), address, destPort);	
		len = sizeof(serverAddr);
		
		// Corpo client
		for(;;)
		{
			fgets(packet.message, MESSAGE_SIZE, stdin); // scanf is never the best option
			if(packet.message[strlen(packet.message) - 1] == '\n') // removes the new line
				packet.message[strlen(packet.message) - 1] = '\0';
			
			// little check
			if(strlen(packet.message) == 0 || (strlen(packet.message) == 1 && (packet.message[0] == '\n' || packet.message[0] == ' ')))
				continue;
			
			time (&rawtime);
			timeinfo = localtime (&rawtime);
			packet.timestamp.day = timeinfo->tm_mday;
			packet.timestamp.month = timeinfo->tm_mon;
			packet.timestamp.year = timeinfo->tm_year + 1900;
			packet.timestamp.hour = timeinfo->tm_hour;
			packet.timestamp.minute = timeinfo->tm_min;
			packet.timestamp.second = timeinfo->tm_sec;
			
			if(sendto(sd, &packet, sizeof(packet), 0, (struct sockaddr*) &serverAddr, len) < 0) {
				perror("An error occurred while trying to send the packet ");
				continue;
			}
		}
	}
	// Children process (Server)
	if(pid == 0)
	{
		int n_read;
		const int on = -1;
		
		struct sockaddr_in clientAddr, serverAddr;
	
		// Server address initialization
		memset ((char *) &serverAddr, 0, sizeof(serverAddr));
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_addr.s_addr = INADDR_ANY;
		serverAddr.sin_port = htons(listenPort);
	
		// Socket creation Creazione socket, set opzioni, binding
		if((sd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
			perror("An error occurred while opening the socket "); 
			exit(1);
		}
		printf("Server (%d): socket created (sd=%d).\n", getpid(), sd);
	
		// Socket option setting
		if(setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0) {
			perror("An error occurred while setting socket options "); 
			exit(1);
		}
		printf("Server (%d): socket options set.\n", getpid());
	
		// Binding of the socket(sd) to the network address
		if(bind(sd, (struct sockaddr *) &serverAddr, sizeof(serverAddr)) < 0) {
			perror("An error occurred while binding the listening socket ");
			exit(1);
		}
		printf("Server (%d): listening socket successfully bounded on port %d\n", getpid(), listenPort);
		len = sizeof(struct sockaddr_in);
		
		
		// Corpo server
		for(;;)
		{
			if(recvfrom(sd, &packet, sizeof(packet), 0, (struct sockaddr*) &clientAddr, &len) < 0) {
				perror("An error occurred while receiving the packet");
				continue;
			}
			printChatMessage(packet, inet_ntoa(clientAddr.sin_addr));
		}
	}
	// Process creation failed
	if(pid < 0)
	{
		perror("Process creation failed");
		exit(1);
	}
	
	return 0;
}


void printFullPacket(DatagramPacket packet, char* senderAddress)
{
	printf("[%d/%d/%d %d:%d:%d] %s: %s\n",
		packet.timestamp.day, packet.timestamp.month, packet.timestamp.year + 1900,
		packet.timestamp.hour, packet.timestamp.minute, packet.timestamp.second,
		senderAddress, packet.message);
}
void printChatMessage(DatagramPacket packet, char* senderAddress)
{
	printf("[%d:%d] %s: %s\n", packet.timestamp.hour, packet.timestamp.minute,
		senderAddress, packet.message);
}

/*
add commands:
/set address
/set port
/block receive

shutdown sockets;
aggiungere comando sul client che cambia ip/porta di invio o porta di ascolto (kill -> signal)
*/
