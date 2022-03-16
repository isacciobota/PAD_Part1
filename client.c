#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <pthread.h>

#define BUFFER_SIZE 256

int sock_desc = 0;
int leave_flag = 0;

void error(char *msg)
{
	perror(msg);
	exit(0);
}

void receive_handler()
{
	char message[BUFFER_SIZE];
	
	bzero(message, BUFFER_SIZE);
	
	while (1)
	{
		int receive = recv(sock_desc, message, BUFFER_SIZE, 0);
		
		if (receive > 0)
		{
			if (strcmp(message, "Server accepts format: m\"Message\"\n") == 0)
			{
				leave_flag = 1;
			}
			printf("%s", message);
		}
		else if (receive == 0)
		{
			break;
		}
		
		bzero(message, BUFFER_SIZE);
	}
}

void send_handler()
{
	char message[BUFFER_SIZE - 1];
	char buffer[BUFFER_SIZE];
	
	while (1)
	{
		fgets(message, BUFFER_SIZE - 1, stdin);
		strcpy(buffer, "m");
		strcat(buffer, message);
	
		if (strncmp(buffer, "m/exit", 6) == 0) 
		{
			send(sock_desc, buffer, strlen(buffer), 0);
			leave_flag = 1;
		}
		else
		{
			send(sock_desc, buffer, strlen(buffer), 0);
		}
		
		bzero(message, BUFFER_SIZE - 1);
		bzero(buffer, BUFFER_SIZE);
	}
}

int main(int argc, char *argv[])
{
	int portno, n;
	struct sockaddr_in server_addr;
	struct hostent *server;
	char buffer[BUFFER_SIZE];
	
	if (argc < 3)
	{
		fprintf(stderr, "usage %s hostname port\n", argv[0]);
		exit(0);
	}
	
	portno = atoi(argv[2]);
	
	sock_desc = socket(AF_INET, SOCK_STREAM, 0);
	
	if (sock_desc < 0)
	{
		error("ERROR opening socket");
	}
	
	server = gethostbyname(argv[1]);
	
	if (server == NULL)
	{
		fprintf(stderr, "ERROR, no such host\n");
		exit(0);
	}
	
	bzero((char *) &server_addr, sizeof(server_addr));
	server_addr.sin_family = AF_INET;
	
	bcopy((char *) server->h_addr, (char *)&server_addr.sin_addr.s_addr, server->h_length);
	
	server_addr.sin_port = htons(portno);
	
	if (connect(sock_desc, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
	{
		error("ERROR connecting");
	}
	
	pthread_t send_thread;
	if (pthread_create(&send_thread, NULL, (void *) &send_handler, NULL) != 0)
	{
		error("ERROR creating thread\n");
	}
	
	pthread_t receive_thread;
	if (pthread_create(&receive_thread, NULL, (void *) &receive_handler, NULL) != 0)
	{
		error("ERROR creating thread\n");
	}
	
	while (1)
	{
		if (leave_flag)
		{
			printf("Leaving the server...\n");
			break;
		}
	}
	
	close(sock_desc);
	
	return 0;
	
}
