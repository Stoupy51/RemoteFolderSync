
#include "c_tcp_manager.h"
#include "../utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#ifdef _WIN32
	int c_winsock_init = 0;
#endif

/**
 * @brief Function that sets up the TCP client.
 * 
 * @param config The configuration of the client.
 * @param tcp_client The TCP client structure to fill.
 * 
 * @return int		0 if the setup was successful, -1 otherwise.
*/
int setup_tcp_client(config_t config, tcp_client_t *tcp_client) {
	
	// Variables
	int code;
	struct sockaddr_in client_addr;

	// Init Winsock if needed
	#ifdef _WIN32

	if (c_winsock_init == 0) {
		WSADATA wsa_data;
		code = WSAStartup(MAKEWORD(2, 2), &wsa_data);
		ERROR_HANDLE_INT_RETURN_INT(code, "setup_tcp_client(): Unable to init Winsock.\n");
		c_winsock_init = 1;
	}

	#endif
	
	// Create the TCP socket
	tcp_client->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	code = tcp_client->socket == INVALID_SOCKET ? -1 : 0;
	ERROR_HANDLE_INT_RETURN_INT(code, "setup_tcp_client(): Unable to create the socket.\n");
	INFO_PRINT("setup_tcp_client(): Socket created.\n");

	// Connect to the server
	client_addr.sin_family = AF_INET;
	client_addr.sin_addr.s_addr = inet_addr(config.ip);
	client_addr.sin_port = htons(config.port);
	code = connect(tcp_client->socket, (struct sockaddr *)&client_addr, sizeof(client_addr));
	ERROR_HANDLE_INT_RETURN_INT(code, "setup_tcp_client(): Unable to connect to the server.\n");
	INFO_PRINT("setup_tcp_client(): Connected to the server.\n");

	// Fill the TCP client structure
	tcp_client->config = config;

	// Return
	return 0;
}

/**
 * @brief Function that runs the TCP client.
 * 
 * @param tcp_client The TCP client structure.
 */
void tcp_client_run(tcp_client_t *tcp_client) {

	// Create the thread that will handle the connections
	pthread_create(&tcp_client->thread, NULL, tcp_client_thread, (thread_param_type)tcp_client);

	// Wait for the thread to end
	pthread_join(tcp_client->thread, NULL);
}

/**
 * @brief Function that handles the TCP client thread.
 * 
 * @param arg The TCP client structure.
 * 
 * @return thread_return_type		0 if the thread ended successfully, -1 otherwise.
*/
thread_return_type tcp_client_thread(thread_param_type arg) {

	// Variables
	//int code = 0;
	tcp_client_t *tcp_client = (tcp_client_t *)arg;
	//byte buffer[1024];

	// Do nothing for now

	// Close the socket and return
	close(tcp_client->socket);
	return 0;
}

