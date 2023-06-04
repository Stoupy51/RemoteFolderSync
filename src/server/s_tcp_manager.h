
#ifndef __SERVER_TCP_MANAGER_H__
#define __SERVER_TCP_MANAGER_H__

#include "../config_manager.h"
#include "../universal_socket.h"
#include "../universal_pthread.h"
#include "../network/net_utils.h"

#define MAX_CLIENTS 32

// Clients view from the server
typedef struct tcp_client_from_server_t {
	SOCKET socket;
	struct sockaddr_in address;
	int id;
} tcp_client_from_server_t;

// Structure for a server thread
typedef struct tcp_server_thread_t {
	SOCKET socket;
	struct sockaddr_in address;

	pthread_t thread;
	pthread_mutex_t mutex;
} tcp_server_thread_t;

// Structure of the TCP server
typedef struct tcp_server_t {

	// Configuration
	config_t config;

	// Threads
	tcp_server_thread_t handle_new_connections;
	tcp_server_thread_t handle_client_requests;

	// Clients
	int clients_count;
	tcp_client_from_server_t clients[MAX_CLIENTS];

} tcp_server_t;

// Function Prototypes
int setup_tcp_server(config_t config, tcp_server_t *tcp_server);
int tcp_server_run(tcp_server_t *tcp_server);
thread_return_type tcp_server_handle_new_connections(thread_param_type arg);
thread_return_type tcp_server_handle_client_requests(thread_param_type arg);

// Internal functions prototypes
int sendAllDirectoryFiles(SOCKET client_socket);
int handle_action_from_client(client_info_t client, message_t *message);


#endif

