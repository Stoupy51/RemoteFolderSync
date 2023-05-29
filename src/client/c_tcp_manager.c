
#include "c_tcp_manager.h"
#include "../utils.h"
#include "../file_watcher.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#ifdef _WIN32
	int c_winsock_init = 0;
#endif

// Global variables
int c_code;
byte c_buffer[8192];
tcp_client_t *g_client;

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
	struct sockaddr_in client_addr;

	// Init Winsock if needed
	#ifdef _WIN32

	if (c_winsock_init == 0) {
		WSADATA wsa_data;
		c_code = WSAStartup(MAKEWORD(2, 2), &wsa_data);
		ERROR_HANDLE_INT_RETURN_INT(c_code, "setup_tcp_client(): Unable to init Winsock.\n");
		c_winsock_init = 1;
	}

	#endif

	// Init mutex
	pthread_mutex_init(&tcp_client->mutex, NULL);
	
	// Create the TCP socket
	tcp_client->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	c_code = tcp_client->socket == INVALID_SOCKET ? -1 : 0;
	ERROR_HANDLE_INT_RETURN_INT(c_code, "setup_tcp_client(): Unable to create the socket.\n");
	INFO_PRINT("setup_tcp_client(): Socket created.\n");

	// Connect to the server
	client_addr.sin_family = AF_INET;
	client_addr.sin_addr.s_addr = inet_addr(config.ip);
	client_addr.sin_port = htons(config.port);
	c_code = connect(tcp_client->socket, (struct sockaddr *)&client_addr, sizeof(client_addr));
	ERROR_HANDLE_INT_RETURN_INT(c_code, "setup_tcp_client(): Unable to connect to the server.\n");
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
 * 
 * @return int		0 if the client ended successfully, -1 otherwise.
 */
int tcp_client_run(tcp_client_t *tcp_client) {

	// Copy to the global variable
	g_client = tcp_client;

	// Create the thread that will handle the connections
	pthread_create(&tcp_client->thread, NULL, tcp_client_thread, NULL);

	// Monitor the directory
	c_code = monitor_directory(
		tcp_client->config.directory,
		on_client_file_created,
		on_client_file_modified,
		on_client_file_deleted,
		on_client_file_renamed
	);
	ERROR_HANDLE_INT_RETURN_INT(c_code, "main(): Failed to monitor the directory");

	// Wait for the thread to end
	pthread_join(tcp_client->thread, NULL);

	// Return
	return 0;
}

/**
 * @brief Function that handles the TCP client thread.
 * 
 * @param arg The TCP client structure.
 * 
 * @return thread_return_type		0 if the thread ended successfully, -1 otherwise.
*/
thread_return_type tcp_client_thread(thread_param_type arg) {

	// Handle parameters
	c_code = arg == NULL ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(c_code, "tcp_client_thread(): Invalid parameters, 'arg' should be NULL.\n");

	// Variables
	message_t message;

	// Get directory files
	getAllDirectoryFiles();

	// TODO : Receive directory changes from the server

	// Disconnect from the server
	message.type = DISCONNECT;
	message.message = NULL;
	message.size = 0;
	STOUPY_CRYPTO(&message, sizeof(message_t), g_client->config.password);
	c_code = socket_write(g_client->socket, &message, sizeof(message_t)) > 0 ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(c_code, "tcp_client_thread(): Unable to send the message.\n");

	// Close the socket and return
	socket_close(g_client->socket);
	return 0;
}

/**
 * @brief Function that gets all the files in the directory from the server.
 * It requests a zip file, and then unzips it.
 * 
 * @return int		0 if the function ended successfully, -1 otherwise.
 */
int getAllDirectoryFiles() {

	// Create the message
	message_t message;
	message.type = GET_ZIP_DIRECTORY;
	message.message = NULL;
	message.size = 0;

	// Variables
	size_t bytes = 0;

	// Send the message through the socket
	STOUPY_CRYPTO(&message, sizeof(message_t), g_client->config.password);
	bytes = socket_write(g_client->socket, &message, sizeof(message_t));
	c_code = bytes > 0 ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(c_code, "getAllDirectoryFiles(): Unable to send the message.\n");

	// Receive the message through the socket
	bytes = socket_read(g_client->socket, &message, sizeof(message_t));
	STOUPY_CRYPTO(&message, sizeof(message_t), g_client->config.password);
	c_code = bytes > 0 ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(c_code, "getAllDirectoryFiles(): Unable to receive the message.\n");

	// Check the message type
	c_code = message.type == SEND_ZIP_DIRECTORY ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(c_code, "getAllDirectoryFiles(): Invalid message type received : %d.\n", message.type);

	///// Receive the zip file
	// Open the zip file
	FILE *fd = fopen(ZIP_TEMPORARY_FILE, "wb");
	ERROR_HANDLE_PTR_RETURN_INT(fd, "getAllDirectoryFiles(): Unable to open the zip file.\n");

	// Receive the zip file
	size_t received_bytes = 0;
	while (received_bytes < message.size) {

		// Read the socket into the c_buffer
		size_t read_size = socket_read(g_client->socket, c_buffer, sizeof(c_buffer));
		STOUPY_CRYPTO(c_buffer, read_size, g_client->config.password);
		c_code = read_size > 0 ? 0 : -1;
		ERROR_HANDLE_INT_RETURN_INT(c_code, "getAllDirectoryFiles(): Unable to receive the zip file.\n");

		// Write the c_buffer into the file
		bytes = fwrite(c_buffer, sizeof(byte), read_size, fd);
		c_code = bytes == read_size ? 0 : -1;
		ERROR_HANDLE_INT_RETURN_INT(c_code, "getAllDirectoryFiles(): Unable to write the zip file.\n");

		// Update the received bytes
		received_bytes += read_size;
	}

	// Close the zip file
	c_code = fclose(fd);
	ERROR_HANDLE_INT_RETURN_INT(c_code, "getAllDirectoryFiles(): Unable to close the zip file.\n");

	// Unzip the zip file into the directory using system()
	char command[1024];
	#ifdef _WIN32
		sprintf(command, "powershell -Command \"Expand-Archive -Path '%s' -DestinationPath '%s' -Force\"", ZIP_TEMPORARY_FILE, g_client->config.directory);
	#else
		sprintf(command, "unzip -o '%s' -d '%s'", ZIP_TEMPORARY_FILE, g_client->config.directory);
	#endif
	c_code = system(command);
	ERROR_HANDLE_INT_RETURN_INT(c_code, "getAllDirectoryFiles(): Unable to unzip the zip file.\n");

	// Remove the zip file
	c_code = remove(ZIP_TEMPORARY_FILE);
	ERROR_HANDLE_INT_RETURN_INT(c_code, "getAllDirectoryFiles(): Unable to remove the zip file.\n");

	// Return
	return 0;
}

/**
 * @brief Function called when a file is created.
 * 
 * @param filepath	Path of the file created
 * 
 * @return int	0 if success, -1 otherwise
 */
int on_client_file_created(const char *filepath) {
	INFO_PRINT("file_created_handler(): File '%s' created.\n", filepath);

	// Lock the mutex
	pthread_mutex_lock(&g_client->mutex);
	int code = 0;
	int filepath_size = strlen(filepath) + 1;

	// Copy the filepath into the c_buffer
	memcpy(c_buffer, filepath, filepath_size);
	STOUPY_CRYPTO(c_buffer, filepath_size, g_client->config.password);

	// Create the message
	message_t message;
	message.type = FILE_CREATED;
	message.message = malloc(filepath_size);
	memcpy(message.message, c_buffer, filepath_size);
	message.size = filepath_size;

	// Send the message through the socket
	STOUPY_CRYPTO(&message, sizeof(message_t), g_client->config.password);
	size_t bytes = socket_write(g_client->socket, &message, sizeof(message_t));
	code = bytes > 0 ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(code, "on_client_file_created(): Unable to send the message.\n");

	// Free the message
	free(message.message);

	///// Read the file
	// Open the file
	int fd = open(filepath, O_RDONLY);
	code = fd > 0 ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(code, "on_client_file_created(): Unable to open the file.\n");

	// Send the size of the file
	size_t file_size = lseek(fd, 0, SEEK_END);
	lseek(fd, 0, SEEK_SET);
	size_t file_size_crypted = file_size;
	STOUPY_CRYPTO(&file_size_crypted, sizeof(size_t), g_client->config.password);
	bytes = socket_write(g_client->socket, &file_size_crypted, sizeof(size_t));
	code = bytes > 0 ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(code, "on_client_file_created(): Unable to send the file size.\n");

	// Send the file
	size_t read_size = 0;
	while (read_size < file_size) {

		// Read the file into the c_buffer
		size_t read_bytes = read(fd, c_buffer, sizeof(c_buffer));
		code = read_bytes > 0 ? 0 : -1;
		ERROR_HANDLE_INT_RETURN_INT(code, "on_client_file_created(): Unable to read the file.\n");

		// Write the c_buffer into the socket
		STOUPY_CRYPTO(c_buffer, read_bytes, g_client->config.password);
		bytes = socket_write(g_client->socket, c_buffer, read_bytes);
		code = bytes > 0 ? 0 : -1;
		ERROR_HANDLE_INT_RETURN_INT(code, "on_client_file_created(): Unable to send the file.\n");

		// Update the read size
		read_size += read_bytes;
	}

	// Close the file
	code = close(fd);
	ERROR_HANDLE_INT_RETURN_INT(code, "on_client_file_created(): Unable to close the file.\n");

	// Unlock the mutex
	pthread_mutex_unlock(&g_client->mutex);

	// Return
	return 0;
}

/**
 * @brief Function called when a file is modified.
 * 
 * @param filepath	Path of the file modified
 * 
 * @return int	0 if success, -1 otherwise
 */
int on_client_file_modified(const char *filepath) {
	INFO_PRINT("file_modified_handler(): File '%s' modified.\n", filepath);
	return 0;
}

/**
 * @brief Function called when a file is deleted.
 * 
 * @param filepath	Path of the file deleted
 * 
 * @return int	0 if success, -1 otherwise
 */
int on_client_file_deleted(const char *filepath) {
	INFO_PRINT("file_deleted_handler(): File '%s' deleted.\n", filepath);
	return 0;
}

/**
 * @brief Function called when a file is renamed.
 * 
 * @param filepath_old	Old path of the file
 * @param filepath_new	New path of the file
 * 
 * @return int	0 if success, -1 otherwise
 */
int on_client_file_renamed(const char *filepath_old, const char *filepath_new) {
	INFO_PRINT("file_renamed_handler(): File '%s' renamed to '%s'.\n", filepath_old, filepath_new);
	return 0;
}
