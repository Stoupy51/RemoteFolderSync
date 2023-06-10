
#include "c_tcp_manager.h"
#include "../global_utils.h"
#include "../file_watcher.h"
#include "../universal_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

#ifdef _WIN32
	int c_winsock_init = 0;
#endif

#define C_BUFFER_SIZE CS_BUFFER_SIZE

// Global variables
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
	memset(&client_addr, 0, sizeof(struct sockaddr_in));
	int code;

	// Fill the TCP client structure
	memset(tcp_client, 0, sizeof(tcp_client_t));
	tcp_client->config = config;

	// Init Winsock if needed
	#ifdef _WIN32

	if (c_winsock_init == 0) {
		WSADATA wsa_data;
		code = WSAStartup(MAKEWORD(2, 2), &wsa_data);
		ERROR_HANDLE_INT_RETURN_INT(code, "setup_tcp_client(): Unable to init Winsock\n");
		c_winsock_init = 1;
	}

	#endif

	// Init mutex
	pthread_mutex_init(&tcp_client->mutex, NULL);
	
	// Create the TCP socket
	tcp_client->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	code = tcp_client->socket == INVALID_SOCKET ? -1 : 0;
	ERROR_HANDLE_INT_RETURN_INT(code, "setup_tcp_client(): Unable to create the socket\n");
	DEBUG_PRINT("setup_tcp_client(): Socket created\n");

	// Connect to the server
	client_addr.sin_family = AF_INET;
	client_addr.sin_addr.s_addr = inet_addr(config.ip);
	client_addr.sin_port = htons(config.port);
	code = connect(tcp_client->socket, (struct sockaddr *)&client_addr, sizeof(client_addr));
	ERROR_HANDLE_INT_RETURN_INT(code, "setup_tcp_client(): Unable to connect to the server\n");
	DEBUG_PRINT("setup_tcp_client(): Connected to the server\n");

	// Receive the zip file
	g_client = tcp_client;
	code = getAllDirectoryFiles();
	ERROR_HANDLE_INT_RETURN_INT(code, "setup_tcp_client(): Failed to receive the zip file\n");

	// Info print
	INFO_PRINT("setup_tcp_client(): Client setup successfully\n");

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

	// Create the thread that will handle the connection with the server
	pthread_create(&tcp_client->thread, NULL, tcp_client_thread, NULL);

	// Monitor the directory
	int code = monitor_directory(
		tcp_client->config.directory,
		on_client_file_created,
		on_client_file_modified,
		on_client_file_deleted,
		on_client_file_renamed
	);
	ERROR_HANDLE_INT_RETURN_INT(code, "main(): Failed to monitor the directory\n");

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
	int code = (arg == NULL) ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(code, "tcp_client_thread(): Invalid parameters, 'arg' should be NULL\n");

	// Variables
	message_t message;
	memset(&message, 0, sizeof(message_t));

	// TODO : Receive directory changes from the server
	while (1);

	// Disconnect from the server
	memset(&message, 0, sizeof(message_t));
	message.type = DISCONNECT;
	ENCRYPT_BYTES(&message, sizeof(message_t), g_client->config.password);
	code = socket_write(g_client->socket, &message, sizeof(message_t), 0) > 0 ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(code, "tcp_client_thread(): Unable to send the disconnect message\n");
	INFO_PRINT("tcp_client_thread(): Disconnected from the server\n");

	// Close the socket and return
	socket_close(g_client->socket);
	return 0;
}

/**
 * @brief Function that gets all the files in the directory from the server.
 * It receives a zip file containing all, and extracts it.
 * 
 * @return int		0 if the function ended successfully, -1 otherwise.
 */
int getAllDirectoryFiles() {

	// Create the message
	message_t message;
	memset(&message, 0, sizeof(message_t));

	// Receive the file size through the socket
	size_t bytes = socket_read(g_client->socket, &message, sizeof(message_t), 0);
	DECRYPT_BYTES(&message, sizeof(message_t), g_client->config.password);
	int code = bytes > 0 ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(code, "getAllDirectoryFiles(): Unable to receive the message\n");

	///// Receive the zip file
	// Open the zip file
	FILE *fd = fopen(ZIP_TEMPORARY_FILE, "wb");
	ERROR_HANDLE_PTR_RETURN_INT(fd, "getAllDirectoryFiles(): Unable to open the zip file\n");

	// Receive the zip file
	byte zip_buffer[C_BUFFER_SIZE];
	ssize_t bytes_remaining = message.size;
	while (bytes_remaining > 0) {

		// Get the size of the buffer
		size_t buffer_size = C_BUFFER_SIZE < bytes_remaining ? C_BUFFER_SIZE : bytes_remaining;

		// Read the file into the buffer
		socket_read(g_client->socket, zip_buffer, buffer_size, 0);
		DECRYPT_BYTES(zip_buffer, buffer_size, g_client->config.password);
		fwrite(zip_buffer, sizeof(byte), buffer_size, fd);

		// Update the bytes remaining
		bytes_remaining -= buffer_size;
	}

	// Close the zip file
	fclose(fd);

	// Unzip the zip file into the directory using system()
	char command[1024];
	#ifdef _WIN32
		sprintf(command, "powershell -Command \"Expand-Archive -Path '%s' -DestinationPath '%s' -Force\"", ZIP_TEMPORARY_FILE, g_client->config.directory);
	#else
		sprintf(command, "unzip -o '%s' -d '%s'", ZIP_TEMPORARY_FILE, g_client->config.directory);
	#endif
	code = system(command);
	ERROR_HANDLE_INT_RETURN_INT(code, "getAllDirectoryFiles(): Unable to unzip the zip file\n");

	// Remove the zip file
	code = remove(ZIP_TEMPORARY_FILE);
	ERROR_HANDLE_INT_RETURN_INT(code, "getAllDirectoryFiles(): Unable to remove the zip file\n");

	// Print the message
	INFO_PRINT("getAllDirectoryFiles(): Directory files received\n");

	// Return
	return 0;
}

/**
 * @brief Function called when a file is created, modified, deleted or renamed.
 * 
 * @param filepath		Path of the file that changed (relative to the directory)
 * @param new_filepath	New path of the file that changed (NULL if the action isn't FILE_RENAMED)
 * @param action		Action that was done on the file
 * 
 * @return int	0 if success, -1 otherwise
 */
int on_client_file_change_handler(const char *filepath, const char *new_filepath, message_type_t action) {

	///// Connect to the server on the send port
	// Create the socket
	SOCKET send_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	int code = send_socket == INVALID_SOCKET ? -1 : 0;
	ERROR_HANDLE_INT_RETURN_INT(code, "on_client_file_change_handler(): Unable to create the socket\n");

	// Create the address
	struct sockaddr_in send_addr;
	memset(&send_addr, 0, sizeof(struct sockaddr_in));
	send_addr.sin_family = AF_INET;
	send_addr.sin_port = htons(g_client->config.port + 1);
	send_addr.sin_addr.s_addr = inet_addr(g_client->config.ip);

	// Connect to the server
	code = connect(send_socket, (struct sockaddr *)&send_addr, sizeof(struct sockaddr_in));
	ERROR_HANDLE_INT_RETURN_INT(code, "on_client_file_change_handler(): Unable to connect to the server\n");
	INFO_PRINT("on_client_file_change_handler(): Connected to the server\n");

	///// Send the message
	// Variables
	byte action_buffer[C_BUFFER_SIZE];
	size_t filepath_size = strlen(filepath) + 1;
	size_t new_filepath_size = new_filepath != NULL ? strlen(new_filepath) + 1 : 0;
	message_t message;
	memset(&message, 0, sizeof(message_t));
	message.type = action;
	message.size = filepath_size;
	size_t bytes;

	// Lock the mutex
	pthread_mutex_lock(&g_client->mutex);
	DEBUG_PRINT("on_client_file_change_handler(): Mutex locked for file '%s'\n", filepath);

	// Send the message through the socket
	ENCRYPT_BYTES(&message, sizeof(message_t), g_client->config.password);
	bytes = socket_write(send_socket, &message, sizeof(message_t), 0);
	code = bytes > 0 ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(code, "on_client_file_change_handler(): Unable to send the message\n");

	// Send the filepath through the socket
	char *filepath_to_send = strdup((char*)filepath);
	ENCRYPT_BYTES(filepath_to_send, filepath_size, g_client->config.password);
	bytes = socket_write(send_socket, filepath_to_send, filepath_size, 0);
	code = bytes > 0 ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(code, "on_client_file_change_handler(): Unable to send the message.message\n");
	DEBUG_PRINT("on_client_file_change_handler(): Message sent\n");
	free(filepath_to_send);

	///// Switch case on the action type
	switch (action) {



		// Send the file content when it's created or modified
		case FILE_CREATED:
		case FILE_MODIFIED:
{
	///// Read the file
	// Get the real filepath
	char real_filepath[2048];
	sprintf(real_filepath, "%s%s", g_client->config.directory, filepath);
	DEBUG_PRINT("on_client_file_change_handler(): Real filepath : '%s'\n", real_filepath);

	// Wait for the file to be fully accessible (60 tries, 1 second each)
	int tries = 60;
	while (tries > 0) {

		// Check if the file is accessible
		if ((code = file_accessible(real_filepath)) == 0)
			break;

		// Else, print a warning and wait
		WARNING_PRINT("on_client_file_created(): File not accessible yet, waiting... (%d tries left)\n", tries);
		tries--;
		sleep(1);
	}
	ERROR_HANDLE_INT_RETURN_INT(code, "on_client_file_created(): File '%s' not accessible\n", real_filepath);

	// Open the file
	FILE *file = fopen(real_filepath, "rb");
	ERROR_HANDLE_PTR_RETURN_INT(file, "on_client_file_change_handler(): Unable to open the file\n");

	// Get the file size
	size_t file_size = get_file_size(fileno(file));
	DEBUG_PRINT("on_client_file_change_handler(): File size : %zu\n", file_size);

	// Send the crypted file size
	size_t file_size_crypted = file_size;
	ENCRYPT_BYTES(&file_size_crypted, sizeof(size_t), g_client->config.password);
	bytes = socket_write(send_socket, &file_size_crypted, sizeof(size_t), 0);
	code = bytes > 0 ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(code, "on_client_file_change_handler(): Unable to send the file size\n");

	// Send the file
	ssize_t bytes_remaining = file_size;
	while (bytes_remaining > 0) {

		// Get the size of the buffer
		size_t buffer_size = C_BUFFER_SIZE < bytes_remaining ? C_BUFFER_SIZE : bytes_remaining;

		// Read the file into the buffer
		fread(action_buffer, sizeof(byte), buffer_size, file);
		ENCRYPT_BYTES(action_buffer, buffer_size, g_client->config.password);
		socket_write(send_socket, action_buffer, buffer_size, 0);

		// Update the bytes remaining
		bytes_remaining -= buffer_size;
	}

	// Info print
	INFO_PRINT("on_client_file_change_handler(): File '%s' sent\n", filepath);

	// Close the file
	fclose(file);
}
			break;





		// As the filepath is already sent, we just do nothing
		case FILE_DELETED:
			break;





		// Send the new filepath
		case FILE_RENAMED:
{
	///// Send the new filepath
	// Send the size of the new filepath
	size_t new_filepath_size_crypted = new_filepath_size;
	ENCRYPT_BYTES(&new_filepath_size_crypted, sizeof(size_t), g_client->config.password);
	bytes = socket_write(send_socket, &new_filepath_size_crypted, sizeof(size_t), 0);
	code = bytes > 0 ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(code, "on_client_file_change_handler(): Unable to send the new filepath size\n");
	DEBUG_PRINT("on_client_file_change_handler(): New filepath size crypted : %zu\n", new_filepath_size);

	// Send the new filepath through the socket
	char *new_filepath_crypted = strdup((char*)new_filepath);
	ENCRYPT_BYTES(new_filepath_crypted, new_filepath_size, g_client->config.password);
	bytes = socket_write(send_socket, new_filepath_crypted, new_filepath_size, 0);
	code = bytes > 0 ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(code, "on_client_file_change_handler(): Unable to send the new filepath\n");

	// Info print
	INFO_PRINT("on_client_file_change_handler(): New filepath sent ('%s' -> '%s')\n", filepath, new_filepath);

	// Free the new filepath
	free(new_filepath_crypted);
}
			break;
		
		default:
			break;
	}

	// Close the socket
	socket_close(send_socket);
	DEBUG_PRINT("on_client_file_change_handler(): Socket closed\n");

	// Unlock the mutex
	pthread_mutex_unlock(&g_client->mutex);
	DEBUG_PRINT("on_client_file_change_handler(): Mutex unlocked\n");

	// Info print
	INFO_PRINT("on_client_file_change_handler(): File change correctly handled\n");

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
	INFO_PRINT("on_client_file_created(): File created : '%s'\n", filepath);
	return on_client_file_change_handler(filepath, NULL, FILE_CREATED);
}

/**
 * @brief Function called when a file is modified.
 * 
 * @param filepath	Path of the file modified
 * 
 * @return int	0 if success, -1 otherwise
 */
int on_client_file_modified(const char *filepath) {
	INFO_PRINT("on_client_file_modified(): File modified : '%s'\n", filepath);
	return on_client_file_change_handler(filepath, NULL, FILE_MODIFIED);
}

/**
 * @brief Function called when a file is deleted.
 * 
 * @param filepath	Path of the file deleted
 * 
 * @return int	0 if success, -1 otherwise
 */
int on_client_file_deleted(const char *filepath) {
	INFO_PRINT("on_client_file_deleted(): File deleted : '%s'\n", filepath);
	return on_client_file_change_handler(filepath, NULL, FILE_DELETED);
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
	INFO_PRINT("file_renamed_handler(): File '%s' renamed to '%s'\n", filepath_old, filepath_new);
	return on_client_file_change_handler(filepath_old, filepath_new, FILE_RENAMED);
}

