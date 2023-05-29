
#include "s_tcp_manager.h"
#include "../utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#ifdef _WIN32
	int s_winsock_init = 0;
#endif

/**
 * @brief Function that sets up the TCP server.
 * 
 * @param config The configuration of the server.
 * @param tcp_server The TCP server structure to fill.
 * 
 * @return int		0 if the setup was successful, -1 otherwise.
*/
int setup_tcp_server(config_t config, tcp_server_t *tcp_server) {
	
	// Variables
	int code;
	struct sockaddr_in server_addr;

	// Init Winsock if needed
	#ifdef _WIN32

	if (s_winsock_init == 0) {
		WSADATA wsa_data;
		code = WSAStartup(MAKEWORD(2, 2), &wsa_data);
		ERROR_HANDLE_INT_RETURN_INT(code, "setup_tcp_server(): Unable to init Winsock.\n");
		s_winsock_init = 1;
	}

	#endif
	
	// Create the TCP socket
	tcp_server->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	code = tcp_server->socket == INVALID_SOCKET ? -1 : 0;
	ERROR_HANDLE_INT_RETURN_INT(code, "setup_tcp_server(): Unable to create the socket.\n");
	INFO_PRINT("setup_tcp_server(): Socket created.\n");

	// Setup the server address
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(config.port);
	
	// Bind the socket to the server address
	code = bind(tcp_server->socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
	ERROR_HANDLE_INT_RETURN_INT(code, "setup_tcp_server(): Unable to bind the socket.\n");
	INFO_PRINT("setup_tcp_server(): Socket binded.\n");
	
	// Listen for connections
	code = listen(tcp_server->socket, 5);
	ERROR_HANDLE_INT_RETURN_INT(code, "setup_tcp_server(): Unable to listen for connections.\n");
	INFO_PRINT("setup_tcp_server(): Listening for connections.\n");

	// Fill the TCP server structure
	tcp_server->config = config;

	// Return
	return 0;
}

/**
 * @brief Function that runs the TCP server.
 * 
 * @param tcp_server The TCP server structure.
 */
void tcp_server_run(tcp_server_t *tcp_server) {

	// Create the thread that will handle the connections
	pthread_create(&tcp_server->thread, NULL, tcp_server_thread, (thread_param_type)tcp_server);

	// Wait for the thread to end
	pthread_join(tcp_server->thread, NULL);
}

/**
 * @brief Function that handles the TCP server thread.
 * 
 * @param arg The TCP server structure.
 * 
 * @return thread_return_type		0 if the thread ended successfully, -1 otherwise.
*/
thread_return_type tcp_server_thread(thread_param_type arg) {

	// Variables
	int code = 0;
	tcp_server_t *tcp_server = (tcp_server_t *)arg;
	struct sockaddr_in client_addr;
	socklen_t client_addr_size = sizeof(struct sockaddr_in);
	int client_socket;
	//byte buffer[1024];

	// Accept connections
	while (code == 0) {

		// Accept the connection
		client_socket = accept(tcp_server->socket, (struct sockaddr *)&client_addr, &client_addr_size);

		// Handle errors
		ERROR_HANDLE_INT_RETURN_INT(client_socket, "tcp_server_thread(): Unable to accept the connection.\n");
		INFO_PRINT("tcp_server_thread(): Connection accepted from %s:%d.\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

		// Add the client to the list
		tcp_client_from_server_t client;
		client.socket = client_socket;
		client.address.sin_addr.s_addr = client_addr.sin_addr.s_addr;
		client.address.sin_family = client_addr.sin_family;
		client.address.sin_port = client_addr.sin_port;
		tcp_server->clients[tcp_server->clients_count] = client;
		tcp_server->clients_count = (tcp_server->clients_count + 1) % MAX_CLIENTS;

		// Do nothing for now
	}

	// Close the socket and return
	close(tcp_server->socket);
	return 0;
}

