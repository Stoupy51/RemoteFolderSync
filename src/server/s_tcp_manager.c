
#include "s_tcp_manager.h"
#include "../global_utils.h"
#include "../universal_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#ifdef _WIN32
	int s_winsock_init = 0;
#endif

#define S_BUFFER_SIZE CS_BUFFER_SIZE

// Global variables
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

	// Local variables
	int code;
	
	// Init Winsock if needed
	#ifdef _WIN32

	if (s_winsock_init == 0) {
		WSADATA wsa_data;
		code = WSAStartup(MAKEWORD(2, 2), &wsa_data);
		ERROR_HANDLE_INT_RETURN_INT(code, "setup_tcp_server(): Unable to init Winsock\n");
		s_winsock_init = 1;
	}

	#endif

	// Fill the TCP server structure
	memset(tcp_server, 0, sizeof(tcp_server_t));
	tcp_server->config = config;

	// Initialize the list of clients to INVALID_SOCKET
	int i = 0;
	for (; i < MAX_CLIENTS; i++)
		tcp_server->clients[i].socket = INVALID_SOCKET;

	///// Create the TCP socket for handling connections
	// Create the TCP socket
	tcp_server->handle_new_connections.socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	code = tcp_server->handle_new_connections.socket == INVALID_SOCKET ? -1 : 0;
	ERROR_HANDLE_INT_RETURN_INT(code, "setup_tcp_server(): Error while creating the socket for handling connections\n");

	// Setup the server address
	struct sockaddr_in *addr = &tcp_server->handle_new_connections.address;
	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = INADDR_ANY;
	addr->sin_port = htons(config.port);

	// Bind the socket to the server address
	code = bind(tcp_server->handle_new_connections.socket, (struct sockaddr *)addr, sizeof(struct sockaddr_in));
	ERROR_HANDLE_INT_RETURN_INT(code, "setup_tcp_server(): Error while binding the socket for handling connections\n");

	// Listen for connections
	code = listen(tcp_server->handle_new_connections.socket, 100);
	ERROR_HANDLE_INT_RETURN_INT(code, "setup_tcp_server(): Error while listening for connections\n");

	// Initialize the mutex
	pthread_mutex_init(&tcp_server->handle_new_connections.mutex, NULL);

	///// Create the TCP socket for handling requests
	// Create the TCP socket
	tcp_server->handle_client_requests.socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	code = tcp_server->handle_client_requests.socket == INVALID_SOCKET ? -1 : 0;
	ERROR_HANDLE_INT_RETURN_INT(code, "setup_tcp_server(): Error while creating the socket for handling requests\n");

	// Setup the server address
	addr = &tcp_server->handle_client_requests.address;
	addr->sin_family = AF_INET;
	addr->sin_addr.s_addr = INADDR_ANY;
	addr->sin_port = htons(config.port + 1);

	// Bind the socket to the server address
	code = bind(tcp_server->handle_client_requests.socket, (struct sockaddr *)addr, sizeof(struct sockaddr_in));
	ERROR_HANDLE_INT_RETURN_INT(code, "setup_tcp_server(): Error while binding the socket for handling requests\n");

	// Listen for connections
	code = listen(tcp_server->handle_client_requests.socket, 100);
	ERROR_HANDLE_INT_RETURN_INT(code, "setup_tcp_server(): Error while listening for connections\n");

	// Initialize the mutex
	pthread_mutex_init(&tcp_server->handle_client_requests.mutex, NULL);

	// Info print
	INFO_PRINT("setup_tcp_server(): TCP server setup successfully\n");

	// Return
	return 0;
}

/**
 * @brief Function that runs the TCP server.
 * 
 * @param tcp_server The TCP server structure.
 * 
 * @return int		0 if the server ended successfully, -1 otherwise.
 */
int tcp_server_run(tcp_server_t *tcp_server) {

	// Copy the TCP server structure to the global variable
	g_server = tcp_server;

	// Create the threads
	pthread_create(&tcp_server->handle_new_connections.thread, NULL, tcp_server_handle_new_connections, NULL);
	pthread_create(&tcp_server->handle_client_requests.thread, NULL, tcp_server_handle_client_requests, NULL);

	// Wait for the threads to end
	pthread_join(tcp_server->handle_new_connections.thread, NULL);
	pthread_join(tcp_server->handle_client_requests.thread, NULL);

	// Return
	return 0;
}

/**
 * @brief Function that handles new connections.
 * It waits for a connection on the socket, sends the directory
 * to the client and then registers the client in the list of clients.
 * 
 * @param arg NULL.
 * 
 * @return thread_return_type		0 if the thread ended successfully, -1 otherwise.
 */
thread_return_type tcp_server_handle_new_connections(thread_param_type arg) {

	// Handle parameters
	int code = (arg == NULL) ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(code, "tcp_server_handle_new_connections(): Invalid parameters, 'arg' should be NULL\n");

	// Variables
	socklen_t client_addr_size = sizeof(struct sockaddr_in);
	char* client_ip = NULL;
	int client_port = 0;

	// Accept connections
	while (g_server->handle_new_connections.socket != INVALID_SOCKET) {

		// Get next client into a variable
		tcp_client_from_server_t *cl = &g_server->clients[g_server->clients_count];

		// Accept the connection
		cl->socket = accept(g_server->handle_new_connections.socket, (struct sockaddr *)&cl->address, &client_addr_size);
		code = cl->socket == INVALID_SOCKET ? -1 : 0;
		ERROR_HANDLE_INT_RETURN_INT(code, "tcp_server_handle_new_connections(): Error while accepting a connection\n");
		cl->id = g_server->clients_count;

		// Get the client IP address and port
		client_ip = inet_ntoa(cl->address.sin_addr);
		client_port = ntohs(cl->address.sin_port);
		INFO_PRINT("tcp_server_handle_new_connections(): Accepted a connection from %s:%d\n", client_ip, client_port);

		// Send the directory to the client
		code = sendAllDirectoryFiles(cl->socket);
		if (code == -1) {
			ERROR_PRINT("tcp_server_handle_new_connections(): Error while sending directory, closing connection with client %s:%d\n", client_ip, client_port);
			continue;
		}

		// Register the client
		INFO_PRINT("tcp_server_handle_new_connections(): Client #%d registered (%s:%d)\n", cl->id, client_ip, client_port);
		g_server->clients_count++;
	}

	// Return
	return 0;
}



/**
 * @brief Function that handles client requests.
 * It accepts a connection from the client, receives the request,
 * send back a response (OK or ERROR) and then closes the connection.
 * 
 * @param arg NULL.
 * 
 * @return thread_return_type		0 if the thread ended successfully, -1 otherwise.
 */
thread_return_type tcp_server_handle_client_requests(thread_param_type arg) {

	// Handle parameters
	int code = (arg == NULL) ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(code, "tcp_server_handle_client_requests(): Invalid parameters, 'arg' should be NULL\n");

	// Variables
	socklen_t client_addr_size = sizeof(struct sockaddr_in);

	// Accept connections
	while (g_server->handle_client_requests.socket != INVALID_SOCKET) {

		// Structure for client info
		client_info_t client;
		memset(&client, 0, sizeof(client_info_t));

		// Accept the connection
		client.socket = accept(g_server->handle_client_requests.socket, (struct sockaddr *)&client.address, &client_addr_size);
		code = (client.socket == INVALID_SOCKET) ? -1 : 0;
		ERROR_HANDLE_INT_RETURN_INT(code, "tcp_server_handle_client_requests(): Error while accepting a connection\n");

		// Get the client IP address and port
		client.ip = inet_ntoa(client.address.sin_addr);
		client.port = ntohs(client.address.sin_port);
		INFO_PRINT("{%s:%d} Connection accepted\n", client.ip, client.port);

		// TODO : Should run a thread for each client

		// Receive the request
		message_t message;
		socket_read(client.socket, &message, sizeof(message_t), 0);
		DECRYPT_BYTES(&message, sizeof(message_t), g_server->config.password);

		// Handle the message
		switch (message.type) {

			case FILE_CREATED:
			case FILE_MODIFIED:
			case FILE_DELETED:
			case FILE_RENAMED:
				code = handle_action_from_client(client, &message);
				break;

			default:
				ERROR_PRINT("{%s:%d} Invalid message type %d\n", client.ip, client.port, message.type);
				code = -1;
				break;
		}

		// Send the response
		memset(&message, 0, sizeof(message_t));
		message.type = (code == 0) ? VALID_RESPONSE : -1;
		ENCRYPT_BYTES(&message, sizeof(message_t), g_server->config.password);
		socket_write(client.socket, &message, sizeof(message_t), 0);

		// Close the connection
		INFO_PRINT("{%s:%d} Connection closed\n", client.ip, client.port);
		socket_close(client.socket);
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
int sendAllDirectoryFiles(SOCKET client_socket) {

	// Create the zip file
	char command[1024];
	#ifdef _WIN32
		sprintf(command, "powershell -Command \"Compress-Archive -Path '%s*' -DestinationPath '%s' -Force\"", g_server->config.directory, ZIP_TEMPORARY_FILE);
	#else
		sprintf(command, "zip -r '%s' '%s*'", ZIP_TEMPORARY_FILE, g_server->config.directory);
	#endif
	int code = system(command);
	ERROR_HANDLE_INT_RETURN_INT(code, "sendAllDirectoryFiles(): Error while creating the zip file\n");

	// Open the zip file
	FILE *zip_file = fopen(ZIP_TEMPORARY_FILE, "rb");
	ERROR_HANDLE_PTR_RETURN_INT(zip_file, "sendAllDirectoryFiles(): Unable to open the zip file\n");

	///// Send the zip file
	// Get the zip file size
	size_t zip_file_size = get_file_size(fileno(zip_file));

	// Send the zip file size
	message_t message;
	memset(&message, 0, sizeof(message_t));
	message.size = zip_file_size;
	ENCRYPT_BYTES(&message, sizeof(message_t), g_server->config.password);
	socket_write(client_socket, &message, sizeof(message_t), 0);

	// Send the zip file
	byte zip_buffer[S_BUFFER_SIZE];
	ssize_t bytes_remaining = zip_file_size;
	while (bytes_remaining > 0) {

		// Get the size of the buffer
		size_t buffer_size = S_BUFFER_SIZE < bytes_remaining ? S_BUFFER_SIZE : bytes_remaining;

		// Read the file into the buffer
		fread(zip_buffer, sizeof(byte), buffer_size, zip_file);
		ENCRYPT_BYTES(zip_buffer, buffer_size, g_server->config.password);
		socket_write(client_socket, zip_buffer, buffer_size, 0);

		// Update the bytes remaining
		bytes_remaining -= buffer_size;
	}

	// Close the zip file
	fclose(zip_file);

	// Delete the zip file
	code = remove(ZIP_TEMPORARY_FILE);
	ERROR_HANDLE_INT_RETURN_INT(code, "sendAllDirectoryFiles(): Unable to delete the zip file\n");

	// Return
	return 0;
}


/**
 * @brief Function that handles the action from the client
 * (send, modify, delete, and rename a file)
 * 
 * @param client	The TCP client structure.
 * @param message	The message received from the client.
 * 
 * @return int		0 if the file was received successfully, -1 otherwise.
 */
int handle_action_from_client(client_info_t client, message_t *message) {

	// Info print
	DEBUG_PRINT("{%s:%d} Handling action %d from client\n", client.ip, client.port, message->type);

	// Variables
	int code = 0;

	// Receive the file name
	char filename[1024];
	memset(filename, 0, sizeof(filename));
	code = socket_read(client.socket, filename, message->size, 0);
	DECRYPT_BYTES(filename, message->size, g_server->config.password);
	ERROR_HANDLE_INT_RETURN_INT(code, "{%s:%d} Error while receiving the file name\n", client.ip, client.port);
	DEBUG_PRINT("{%s:%d} Received file name '%s'\n", client.ip, client.port, filename);

	// Get the file path
	char filepath[1024];
	sprintf(filepath, "%s%s", g_server->config.directory, filename);
	DEBUG_PRINT("{%s:%d} File path '%s'\n", client.ip, client.port, filepath);

	// Switch case on the message type (action)
	switch (message->type) {



		// Send the file content when it's created or modified
		case FILE_CREATED:
		case FILE_MODIFIED:
{
	// Info print
	INFO_PRINT("{%s:%d} Receiving file '%s'\n", client.ip, client.port, filename);

	///// Read the file content
	// Receive the file size
	size_t file_size = 0;
	code = socket_read(client.socket, &file_size, sizeof(size_t), 0) > 0 ? 0 : -1;
	DECRYPT_BYTES(&file_size, sizeof(size_t), g_server->config.password);
	ERROR_HANDLE_INT_RETURN_INT(code, "{%s:%d} Error while receiving the file size\n", client.ip, client.port);
	DEBUG_PRINT("{%s:%d} Received file size '%zu'\n", client.ip, client.port, file_size);

	// Open the file
	FILE *file = fopen(filepath, "wb");
	ERROR_HANDLE_PTR_RETURN_INT(file, "{%s:%d} Unable to open the file\n", client.ip, client.port);

	// Buffer to receive the file
	byte action_buffer[S_BUFFER_SIZE];
	memset(action_buffer, 0, S_BUFFER_SIZE);

	// Receive the file
	ssize_t bytes_remaining = file_size;
	while (bytes_remaining > 0) {

		// Get the buffer size
		size_t buffer_size = S_BUFFER_SIZE < bytes_remaining ? S_BUFFER_SIZE : bytes_remaining;

		// Read the file from the socket into the file
		socket_read(client.socket, action_buffer, buffer_size, 0);
		DECRYPT_BYTES(action_buffer, buffer_size, g_server->config.password);
		fwrite(action_buffer, sizeof(byte), buffer_size, file);

		// Update the bytes remaining
		bytes_remaining -= buffer_size;
	}

	// Close the file
	fclose(file);
	INFO_PRINT("{%s:%d} File '%s' correctly received\n", client.ip, client.port, filename);
}
			break;





		// Delete the file
		case FILE_DELETED:
{
	// Info print
	INFO_PRINT("{%s:%d} Deleting file '%s'\n", client.ip, client.port, filename);

	// Delete the file
	code = remove(filepath);
	if (code == 0) {
		INFO_PRINT("{%s:%d} File '%s' correctly deleted\n", client.ip, client.port, filename);
	}
	else {
		code = remove_directory(filepath);
		if (code == 0) {
			INFO_PRINT("{%s:%d} Folder '%s' correctly deleted\n", client.ip, client.port, filename);
		}
		else {
			WARNING_PRINT("{%s:%d} Unable to delete folder '%s'\n", client.ip, client.port, filename);
		}
	}
}
			break;





		// Rename the file
		case FILE_RENAMED:
{
	///// Receive the new file name
	// Receive the new file name size
	size_t new_filename_size = 0;
	code = socket_read(client.socket, &new_filename_size, sizeof(size_t), 0) > 0 ? 0 : -1;
	DECRYPT_BYTES(&new_filename_size, sizeof(size_t), g_server->config.password);
	ERROR_HANDLE_INT_RETURN_INT(code, "{%s:%d} Error while receiving the new file name size\n", client.ip, client.port);
	DEBUG_PRINT("{%s:%d} Received new file name size '%zu'\n", client.ip, client.port, new_filename_size);

	// Receive the new file name
	char new_filename[1024];
	code = socket_read(client.socket, new_filename, sizeof(new_filename), 0) > 0 ? 0 : -1;
	DECRYPT_BYTES(new_filename, sizeof(new_filename), g_server->config.password);
	ERROR_HANDLE_INT_RETURN_INT(code, "{%s:%d} Error while receiving the new file name\n", client.ip, client.port);

	// Info print
	INFO_PRINT("{%s:%d} Renaming file '%s' to '%s'\n", client.ip, client.port, filename, new_filename);

	// Get the new file path
	char new_filepath[1024];
	sprintf(new_filepath, "%s%s", g_server->config.directory, new_filename);

	// Rename the file
	code = rename(filepath, new_filepath);
	if (code == 0) {
		INFO_PRINT("{%s:%d} File '%s' correctly renamed to '%s'\n", client.ip, client.port, filename, new_filepath);
	}
	else {
		WARNING_PRINT("{%s:%d} Unable to rename file '%s'\n", client.ip, client.port, filename);
	}
}
			break;
		
		default:
			break;
	}

	// Return
	return 0;
}

