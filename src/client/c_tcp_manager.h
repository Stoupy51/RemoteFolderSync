
#ifndef __CLIENT_TCP_MANAGER_H__
#define __CLIENT_TCP_MANAGER_H__

#include "../config_manager.h"
#include "../universal_socket.h"
#include "../universal_pthread.h"

// Structure of the TCP client
typedef struct {
	config_t config;

	SOCKET socket;
	pthread_t thread;

	struct sockaddr_in address;

} tcp_client_t;

// Function Prototypes
int setup_tcp_client(config_t config, tcp_client_t *tcp_client);
thread_return_type tcp_client_thread(thread_param_type arg);
void tcp_client_run(tcp_client_t *tcp_client);

#endif

