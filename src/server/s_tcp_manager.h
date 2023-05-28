
#ifndef __SERVER_TCP_MANAGER_H__
#define __SERVER_TCP_MANAGER_H__

#include "../config_manager.h"
#include "../universal_socket.h"
#include "../universal_pthread.h"

#define MAX_CLIENTS 8

// Clients view from the server
typedef struct {
	SOCKET socket;
	struct sockaddr_in address;
} tcp_client_from_server_t;

// Structure of the TCP server
typedef struct {
	config_t config;

	SOCKET socket;
	pthread_t thread;

	int clients_count;
	tcp_client_from_server_t clients[MAX_CLIENTS];

} tcp_server_t;

// Function Prototypes
int setup_tcp_server(config_t config, tcp_server_t *tcp_server);
thread_return_type tcp_server_thread(thread_param_type arg);
void tcp_server_run(tcp_server_t *tcp_server);


#endif

