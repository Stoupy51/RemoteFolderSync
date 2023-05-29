
#include "s_tcp_manager.h"
#include "../utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#ifdef _WIN32
	int s_winsock_init = 0;
#endif

// Global variables
int s_code;
byte s_buffer[8192];
tcp_server_t *g_server;

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
	struct sockaddr_in server_addr;

	// Init Winsock if needed
	#ifdef _WIN32

	if (s_winsock_init == 0) {
		WSADATA wsa_data;
		s_code = WSAStartup(MAKEWORD(2, 2), &wsa_data);
		ERROR_HANDLE_INT_RETURN_INT(s_code, "setup_tcp_server(): Unable to init Winsock.\n");
		s_winsock_init = 1;
	}

	#endif
	
	// Create the TCP socket
	tcp_server->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	s_code = tcp_server->socket == INVALID_SOCKET ? -1 : 0;
	ERROR_HANDLE_INT_RETURN_INT(s_code, "setup_tcp_server(): Unable to create the socket.\n");
	INFO_PRINT("setup_tcp_server(): Socket created.\n");

	// Setup the server address
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;
	server_addr.sin_port = htons(config.port);

	// Bind the socket to the server address
	s_code = bind(tcp_server->socket, (struct sockaddr *)&server_addr, sizeof(server_addr));
	ERROR_HANDLE_INT_RETURN_INT(s_code, "setup_tcp_server(): Unable to bind the socket.\n");
	INFO_PRINT("setup_tcp_server(): Socket binded.\n");

	// Listen for connections
	s_code = listen(tcp_server->socket, 5);
	ERROR_HANDLE_INT_RETURN_INT(s_code, "setup_tcp_server(): Unable to listen for connections.\n");
	INFO_PRINT("setup_tcp_server(): Listening for connections.\n");

	// Fill the TCP server structure
	tcp_server->config = config;

	// Set the clients count to 0
	tcp_server->clients_count = 0;

	// Return
	return 0;
}

/**
 * @brief Function that runs the TCP server.
 * 
 * @param tcp_server The TCP server structure.
 */
void tcp_server_run(tcp_server_t *tcp_server) {

	// Copy the TCP server structure to the global variable
	g_server = tcp_server;

	// Create the thread that will handle the connections
	pthread_create(&tcp_server->thread, NULL, tcp_server_thread, NULL);

	// Wait for the thread to end
	pthread_join(tcp_server->thread, NULL);
}

/**
 * @brief Function that handles the TCP server thread.
 * 
 * @param arg NULL.
 * 
 * @return thread_return_type		0 if the thread ended successfully, -1 otherwise.
*/
thread_return_type tcp_server_thread(thread_param_type arg) {

	// Handle parameters
	s_code = arg == NULL ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(s_code, "tcp_client_thread(): Invalid parameters, 'arg' should be NULL.\n");

	// Variables
	int run = 0;
	socklen_t client_addr_size = sizeof(struct sockaddr_in);

	// Accept connections
	while (run == 0) {

		// Accept the connection
		tcp_client_from_server_t *cl = &g_server->clients[g_server->clients_count];
		INFO_PRINT("tcp_server_thread(): Waiting for a connection...\n");
		cl->socket = accept(g_server->socket, (struct sockaddr *)&cl->address, &client_addr_size);
		INFO_PRINT("tcp_server_thread(): Connection accepted.\n");
		g_server->clients_count = (g_server->clients_count + 1) % MAX_CLIENTS;

		// Handle errors
		s_code = cl->socket == INVALID_SOCKET ? -1 : 0;
		ERROR_HANDLE_INT_RETURN_INT(s_code, "tcp_server_thread(): Unable to accept the connection.\n");
		INFO_PRINT("tcp_server_thread(): Connection accepted from %s:%d.\n", inet_ntoa(cl->address.sin_addr), ntohs(cl->address.sin_port));

		// Create the thread that will handle the client
		INFO_PRINT("tcp_server_thread(): Creating thread for the client #%d.\n", g_server->clients_count);
		pthread_create(&cl->thread, NULL, tcp_client_thread_from_server, (thread_param_type)&(*cl));
		INFO_PRINT("tcp_server_thread(): Thread created for the client #%d.\n", g_server->clients_count);
	}

	// Close the socket and return
	close(g_server->socket);
	return 0;
}

/**
 * @brief Function that manage a TCP client.
 * 
 * @param arg	The TCP client structure.
 * 
 * @return thread_return_type		0 if the thread ended successfully, -1 otherwise.
 */
thread_return_type tcp_client_thread_from_server(thread_param_type arg) {

	// Variables
	tcp_client_from_server_t *client = (tcp_client_from_server_t *)arg;
	INFO_PRINT("tcp_client_thread(): Thread started for the client.\n");

	// Receive the message from the client while the client is connected
	message_t message;
	while (client->socket != 0) {
		
		// Receive the message
		size_t read_size = read(client->socket, &message, sizeof(message_t));
		s_code = read_size == 0 ? -1 : 0;
		ERROR_HANDLE_INT_RETURN_INT(s_code, "tcp_client_thread(): Unable to read the message.\n");

		// Handle the message
		switch (message.type) {

			case GET_ZIP_DIRECTORY:
				INFO_PRINT("tcp_client_thread(): GET_ZIP_DIRECTORY message received.\n");
				sendAllDirectoryFiles(client);
				break;

			default:
				INFO_PRINT("tcp_client_thread(): Unknown message type %d.\n", message.type);
				break;
		}

		// Reset the message
		memset(&message, 0, sizeof(message_t));
	}

	// Return
	return 0;
}


/**
 * @brief Function that sends the directory files to the client as a zip file.
 * 
 * @param client	The TCP client structure.
 * 
 * @return int		0 if the message was sent successfully, -1 otherwise.
 */
int sendAllDirectoryFiles(tcp_client_from_server_t *client) {

	// Variables

	// Create the zip file
	char command[1024];
	sprintf(command, "zip -r '%s' '%s'", ZIP_TEMPORARY_FILE, g_server->config.directory);
	s_code = system(command);
	ERROR_HANDLE_INT_RETURN_INT(s_code, "sendAllDirectoryFiles(): Unable to create the zip file.\n");

	// Open the zip file
	FILE *zip_file = fopen(ZIP_TEMPORARY_FILE, "rb");
	ERROR_HANDLE_PTR_RETURN_INT(zip_file, "sendAllDirectoryFiles(): Unable to open the zip file.\n");

	///// Send the zip file
	// Get the zip file size from FILE structure
	s_code = fseek(zip_file, 0, SEEK_END);
	ERROR_HANDLE_INT_RETURN_INT(s_code, "sendAllDirectoryFiles(): Unable to seek the zip file (1).\n");
	size_t zip_file_size = ftell(zip_file);
	s_code = fseek(zip_file, 0, SEEK_SET);
	ERROR_HANDLE_INT_RETURN_INT(s_code, "sendAllDirectoryFiles(): Unable to seek the zip file (2).\n");

	// Send the zip file size
	message_t message;
	message.type = SEND_ZIP_DIRECTORY;
	message.message = NULL;
	message.size = zip_file_size;
	s_code = write(client->socket, &message, sizeof(message_t)) == sizeof(message_t) ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(s_code, "sendAllDirectoryFiles(): Unable to send the zip file size.\n");

	// Send the zip file
	while (zip_file_size > 0) {

		// Read the zip file
		size_t read_size = fread(s_buffer, sizeof(byte), sizeof(s_buffer), zip_file);
		s_code = read_size > 0 ? 0 : -1;
		ERROR_HANDLE_INT_RETURN_INT(s_code, "sendAllDirectoryFiles(): Unable to read the zip file.\n");

		// Send the zip file
		size_t written = write(client->socket, s_buffer, read_size);
		s_code = (written == read_size) ? 0 : -1;
		ERROR_HANDLE_INT_RETURN_INT(s_code, "sendAllDirectoryFiles(): Unable to send the zip file.\n");

		// Update the zip file size
		zip_file_size -= read_size;
	}

	// Close the zip file
	s_code = fclose(zip_file);
	ERROR_HANDLE_INT_RETURN_INT(s_code, "sendAllDirectoryFiles(): Unable to close the zip file.\n");

	// Delete the zip file
	// s_code = remove(ZIP_TEMPORARY_FILE);
	// ERROR_HANDLE_INT_RETURN_INT(s_code, "sendAllDirectoryFiles(): Unable to delete the zip file.\n");

	// Return
	return 0;
}


