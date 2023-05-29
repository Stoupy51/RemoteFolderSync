
#ifndef __CLIENT_TCP_MANAGER_H__
#define __CLIENT_TCP_MANAGER_H__

#include "../config_manager.h"
#include "../universal_socket.h"
#include "../universal_pthread.h"
#include "../network/net_utils.h"

// Structure of the TCP client
typedef struct {
	config_t config;

	SOCKET socket;
	pthread_t thread;
	pthread_mutex_t mutex;

	struct sockaddr_in address;

} tcp_client_t;

// Function Prototypes
int setup_tcp_client(config_t config, tcp_client_t *tcp_client);
void tcp_client_run(tcp_client_t *tcp_client);
thread_return_type tcp_client_thread(thread_param_type arg);

// Internal functions prototypes
int getAllDirectoryFiles();
int on_client_file_created(const char *filepath);
int on_client_file_modified(const char *filepath);
int on_client_file_deleted(const char *filepath);
int on_client_file_renamed(const char *filepath_old, const char *filepath_new);

#endif

