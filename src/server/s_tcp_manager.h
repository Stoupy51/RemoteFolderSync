
#ifndef __SERVER_TCP_MANAGER_H__
#define __SERVER_TCP_MANAGER_H__

#include "../config_manager.h"
#include "../universal_socket.h"
#include "../universal_pthread.h"
#include "../network/net_utils.h"

#define MAX_CLIENTS 8

// Clients view from the server
typedef struct tcp_client_from_server_t {

	// Client socket
	SOCKET socket;
	struct sockaddr_in address;

	// Client thread
	pthread_t thread;

} tcp_client_from_server_t;

// Structure of the TCP server
typedef struct tcp_server_t {

	// Configuration
	config_t config;

	// Server socket and thread
	SOCKET socket;
	pthread_t thread;

	// Clients
	int clients_count;
	tcp_client_from_server_t clients[MAX_CLIENTS];

} tcp_server_t;

// Function Prototypes
int setup_tcp_server(config_t config, tcp_server_t *tcp_server);
void tcp_server_run(tcp_server_t *tcp_server);
thread_return_type tcp_server_thread(thread_param_type arg);
thread_return_type tcp_client_thread_from_server(thread_param_type arg);

// Internal functions prototypes
int sendAllDirectoryFiles(tcp_client_from_server_t *client);


#endif

