#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_CLIENTS 100
#define BUFFER_SIZE 256

static int nr_clients = 0;
static int id = 0;

typedef struct{
	struct sockaddr_in client_addr;
	int socket_desc;
	int id;
}client;

client *clients[MAX_CLIENTS];

pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void error(char *msg)
{
	perror(msg);
	exit(1);
}

void queue_add(client *cl)
{
	int i;
	
	pthread_mutex_lock(&clients_mutex);
	
	for (i = 0; i < MAX_CLIENTS;  i++)
	{
		if (!clients[i])
		{
			clients[i] = cl;
			break;
		}
	}
	
	pthread_mutex_unlock(&clients_mutex);
}

void queue_remove(int id)
{
	int i;
	
	pthread_mutex_lock(&clients_mutex);
	
	for (i = 0; i < MAX_CLIENTS;  i++)
	{
		if (clients[i] && clients[i]->id == id)
		{
			clients[i] = NULL;
			break;
		}
	}
	
	pthread_mutex_unlock(&clients_mutex);
}

void send_message(int id, char message[BUFFER_SIZE])
{
	int i;
	
	pthread_mutex_lock(&clients_mutex);
	
	for (i = 0; i < MAX_CLIENTS;  i++)
	{
		if (clients[i] && clients[i]->id != id)
		{
			if (write(clients[i]->socket_desc, message, strlen(message)) < 0)
			{
				printf("ERROR in sending message\n");
				break;
			}
		}
	}
	
	pthread_mutex_unlock(&clients_mutex);
}

void *connection_handler(void *socket_desc)
{
	char buffer[BUFFER_SIZE];
	int leave_flag = 0;
	
	nr_clients++;
	
	client *c = (client *)socket_desc;
	
	sprintf(buffer, "Id %d has joined\n", c->id);
	printf("%s", buffer);
	send_message(c->id, buffer);
	
	bzero(buffer, BUFFER_SIZE);
	
	while (1)
	{

		if (leave_flag)
			break;
		
		int receive = recv(c->socket_desc, buffer, BUFFER_SIZE, 0);
		
		if (receive > 0)
		{
			if (buffer[0] == 'm')
			{
				if (strncmp(buffer, "m/exit", 6) == 0)
				{
					sprintf(buffer, "Id %d has left\n", c->id);
					printf("%s", buffer);
					send_message(c->id, buffer);
					leave_flag = 1;
				}
				else
				{
					char s[BUFFER_SIZE];
					char id_[10];
					sprintf(id_, "%d", c->id);
					strcpy(s, "Id ");
					strcat(s, id_);
					strcat(s, ":");
					strcat(s, buffer + 1);
					send_message(c->id, s);
				}
			}
			else
			{
				pthread_mutex_lock(&clients_mutex);
			
				char message[BUFFER_SIZE] = "Server accepts format: m\"Message\"\n";
				if (write(clients[c->id]->socket_desc, message, strlen(message)) < 0)
				{
					printf("ERROR in sending message\n");
					break;
				}
				
				pthread_mutex_unlock(&clients_mutex);
				
				sprintf(buffer, "Id %d has left\n", c->id);
				printf("%s", buffer);
				send_message(c->id, buffer);
				leave_flag = 1;

			}
		}
		
		if (receive < 0)
		{
			error("ERROR\n");
			leave_flag = 1;
		}
		
		bzero(buffer, BUFFER_SIZE);
	}
	
	close(c->socket_desc);
	queue_remove(c->id);
	free(c);
	nr_clients--;
	
	pthread_detach(pthread_self());
	
	return 0;
}

int main(int argc, char *argv[])
{
	int socket_desc, portno, socket_client, client_s, n, *socket_clnt;
	char buffer[256];
	struct sockaddr_in server_addr, client_addr;
	
	if (argc < 2)
	{
		fprintf(stderr, "No port provided\n");
	}
	
	socket_desc = socket(AF_INET, SOCK_STREAM, 0);
	
	if (socket_desc == -1)
	{
		error("Could not create socket");
	}
	
	bzero((char *) &server_addr, sizeof(server_addr));
	
	portno = atoi(argv[1]);
	
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(portno);
	
	if (bind(socket_desc, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
	{
		error("connect error");
	}
	
	client_s = sizeof(client_addr);
	
	if (listen(socket_desc, MAX_CLIENTS) == 0)
	{
		printf("Listening\n");
	} 
	else 
	{
		printf("ERROR");
	}
	
	while (1)
	{
		socket_client = accept(socket_desc, (struct sockaddr *) &client_addr, (socklen_t*)&client_s);
		
		if((nr_clients + 1) == MAX_CLIENTS)
		{
			printf("Maximum clients connected. Connection rejected");
			close(socket_client);
			continue;
		}
		
		client *c = (client *)malloc(sizeof(client));
		c->client_addr = client_addr;
		c->socket_desc = socket_client;
		c->id = id++;
		
		pthread_t thread;
		
		queue_add(c);
		pthread_create(&thread, NULL, &connection_handler, (void *)c);
		
		sleep(1);
	}
	
	return 0;
}
