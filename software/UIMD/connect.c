
#include "connect.h"

extern pthread_t threads[MAX_CLIENTS];
extern void* sent_to_client(void *idx);
extern pthread_mutex_t log_msg;
extern int   client_socks[MAX_CLIENTS];
extern short is_client_connected[MAX_CLIENTS];
extern short terminate_thread;
extern char clients[MAX_CLIENTS][STRING_LENGTH];
extern int server_port;

int client_count = 0;
pthread_t accept_thread;

/* checks if the current incoming client request is from a known IP address;
	returns 1 if IP is valid; 0 otherwise
*/
short is_legitimate_client(const char* pclient_ip)
{
	short i = 0, ret_val = 0;

	for(i = 0; i < MAX_CLIENTS; ++i)
	{
		if(!strcmp(pclient_ip, clients[i]))
		{
			pthread_mutex_lock(&log_msg);
			log_message("Legitimate connection request");
			pthread_mutex_unlock(&log_msg);
			ret_val = 1;
			break;
		}
	}

	if(ret_val == 0)
	{
		pthread_mutex_lock(&log_msg);
		log_message("***Connection request received from an unknown IP ***");
		pthread_mutex_unlock(&log_msg);
	}
	return ret_val;
}

/* thread function to accept incoming uimd client connections */
void* accept_loop(void *serversock)
{
	struct sockaddr_in clientsocaddr;
	unsigned int clientlen;
	int rc, clientsock = -1;
	static int thread_index;

	pthread_mutex_lock(&log_msg);
	log_message("*** Ready to accept an incoming connection ***");
	pthread_mutex_unlock(&log_msg);

	while(1)
	{
		clientsock = -1;

		/* wait for incoming uimd client connection */
		clientlen = sizeof(clientsocaddr);
		errno = 0;
		if((clientsock = accept(*((int*) serversock), (struct sockaddr *) &clientsocaddr, &clientlen)) < 0)
		{
			pthread_mutex_lock(&log_msg);
			log_message("Failed to accept server connection");
			log_message(strerror(errno));
			pthread_mutex_unlock(&log_msg);
			break;
		}

		pthread_mutex_lock(&log_msg);
		log_message("Connected to Client ip:");
		log_message(inet_ntoa(clientsocaddr.sin_addr));
		pthread_mutex_unlock(&log_msg);

		if(is_legitimate_client(inet_ntoa(clientsocaddr.sin_addr)) && client_count < MAX_CLIENTS)
		{
			is_client_connected[client_count] = 1;
			client_socks[client_count] = clientsock;

			/* create thread which sends data to this client */
			thread_index = client_count;
			rc = pthread_create(&threads[thread_index], NULL, sent_to_client, (void *)&thread_index);
			if(rc)
			{
				pthread_mutex_lock(&log_msg);
				log_message("Failed to create pthread for client");
				pthread_mutex_unlock(&log_msg);
			}	

			++client_count;
		}
		else
		{
			pthread_mutex_lock(&log_msg);
			log_message("*** MAX count for connected clients reached. Try increasing MAX_CLIENTS. ***");
			pthread_mutex_unlock(&log_msg);
		}

		if(terminate_thread)
			break;

		usleep(MSEC * 100);
	}

	pthread_mutex_lock(&log_msg);
	log_message("*** Connect thread exiting ***");
	pthread_mutex_unlock(&log_msg);

	close(*((int*)serversock));
	pthread_exit(NULL);
}

int server_socket = INVALID_SOCKET;
/* main function to handle incoming client connection requests */
void server_connect(const char* server_ip)
{
	static int serversock;
	int i, rc;
	struct sockaddr_in serversockaddr;

	/* initialize */
	for(i = 0; i < MAX_CLIENTS; ++i)
	{
		is_client_connected[i] = 0;
		client_socks[i] = INVALID_SOCKET;
	}

	errno = 0;
	if((serversock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
	{
		log_message("*** Failed to create server socket ***");
		log_message(strerror(errno));
		return;
	}
	log_message("Server socket created");

	memset(&serversockaddr, 0, sizeof(serversockaddr));  /* Clear struct */
	serversockaddr.sin_family = AF_INET;                 /* Internet/IP */
	serversockaddr.sin_addr.s_addr = htonl(INADDR_ANY);
	/*serversockaddr.sin_port = htons(SERVER_PORT);*/        /* Server Port */
	serversockaddr.sin_port = htons(server_port);        /* Server Port */

	/* Bind the server socket */
	errno = 0;
	if(bind(serversock, (struct sockaddr *) &serversockaddr, sizeof(serversockaddr)) < 0)
	{
		log_message("*** Failed to bind the server socket ***");
		log_message(strerror(errno));
		close(serversock);
		return;
	}
	log_message("Bind successful");

	/* Listen on the server socket */
	errno = 0;
	log_message("Starting to listen");
	if(listen(serversock, MAX_CLIENTS) < 0)
	{
		log_message("Failed to listen on server socket");
		log_message(strerror(errno));
		close(serversock);
		return;
	}

	server_socket = serversock;
	rc = pthread_create(&accept_thread, NULL, accept_loop, &serversock);
	if(rc != 0)
	{
		log_message("*** Failed to server accept loop thread ***");
		close(serversock);
	}
	else
		log_message("Server accept loop thread created");
}

