//
//  Client.cpp
//  socketwebclient
//
//  Created by Hazert on 16/4/22.
//  Copyright © 2016年 Hazert. All rights reserved.
//
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include "encryption.h"
#include <stdint.h>

#define PORT "5000" // the port client will be connecting to 

#define MAXDATASIZE 100 // max number of bytes we can get at once 

char seMessage[100];
char reMessage[100];

int msg_len;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
	if (sa->sa_family == AF_INET) {
		return &(((struct sockaddr_in*)sa)->sin_addr);
	}

	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// send information depends on the encryption situation
void sendText(int sockfd, char *msg, int isSecret) {
	int len = strlen(msg);
	msg[len - 1] = '\0';
	len = strlen(msg);
	char newMsg[len + 1];
	// according to isSecret code decide whether to do encryption
	if (isSecret == 0) { 
		sprintf(newMsg, "%s%s", msg, "0");
	} else {
		for(int i = 0;i < strlen(msg);i++) {
			msg[i] = encry((int)msg[i]);
		}
		sprintf(newMsg, "%s%s", msg, "1");  // add encryption bit
	}
	send(sockfd, newMsg, strlen(newMsg), 0); 
}

/* function for receiving information and detect the encryption situation*/

void recvInfo(char *msg) {
	int len = strlen(msg);
	// receive normally request 
	if (msg[len - 1] == '0') { 
		msg[len - 1] = '\0';
		printf("%s", msg);
	}else {
		// if not secret information, need decryption
		msg[len - 1] = '\0';		
		for (int j = 0;j < strlen(msg);j++) {
			printf("%c", decry(msg[j]));
		}
		printf("\n");	
	}
}

int main(int argc, char *argv[])
{
	int sockfd, numbytes;  
	char buf[MAXDATASIZE];
	struct addrinfo hints, *servinfo, *p;
	int rv;
	char s[INET6_ADDRSTRLEN];

	if (argc != 2) {
		fprintf(stderr,"usage: client hostname\n");
		exit(1);
	}

	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return 1;
	}
	
	// loop through all the results and connect to the first we can
	for(p = servinfo; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("client: connect");
			continue;
		}
		break;
	}

	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return 2;
	}

	inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
	printf("client: connecting to %s\n", s);

	freeaddrinfo(servinfo); // all done with this structure

	if ((numbytes = recv(sockfd, buf, MAXDATASIZE-1, 0)) == -1) {
		perror("recv");
		exit(1);
	}

	buf[numbytes] = '\0';

	printf("client: received '%s'\n",buf);
	
	for(int i = 0; i<2; i++){
		pid_t fpid = fork();
		if (fpid == 0){
			while (1) {
				fgets(seMessage, sizeof(seMessage), stdin);	
				
				if (seMessage[0] == '\\') {
					sendText(sockfd, seMessage, 0);	
				} else {
					sendText(sockfd, seMessage, 1);
				}	
			}	
		}else{
			while (1) {
				recv(sockfd, reMessage, sizeof(reMessage), 0);
				recvInfo(reMessage);
				memset(reMessage, '\0', sizeof(reMessage));
			}
		}
	}
		
	close(sockfd);
	
	return 0;
}