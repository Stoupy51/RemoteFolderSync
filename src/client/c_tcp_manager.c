
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
byte c_buffer[CS_BUFFER_SIZE];
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
		ERROR_HANDLE_INT_RETURN_INT(c_code, "setup_tcp_client(): Unable to init Winsock\n");
		c_winsock_init = 1;
	}

	#endif

	// Init mutex
	pthread_mutex_init(&tcp_client->mutex, NULL);

	// Init condition
	pthread_cond_init(&tcp_client->zip_file_received, NULL);
	
	// Create the TCP socket
	tcp_client->socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	c_code = tcp_client->socket == INVALID_SOCKET ? -1 : 0;
	ERROR_HANDLE_INT_RETURN_INT(c_code, "setup_tcp_client(): Unable to create the socket\n");
	INFO_PRINT("setup_tcp_client(): Socket created\n");

	// Connect to the server
	client_addr.sin_family = AF_INET;
	client_addr.sin_addr.s_addr = inet_addr(config.ip);
	client_addr.sin_port = htons(config.port);
	c_code = connect(tcp_client->socket, (struct sockaddr *)&client_addr, sizeof(client_addr));
	ERROR_HANDLE_INT_RETURN_INT(c_code, "setup_tcp_client(): Unable to connect to the server\n");
	INFO_PRINT("setup_tcp_client(): Connected to the server\n");

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

	// Wait for the thread to receive the zip file
	pthread_mutex_lock(&tcp_client->mutex);
	pthread_cond_wait(&tcp_client->zip_file_received, &tcp_client->mutex);
	pthread_mutex_unlock(&tcp_client->mutex);

	// Monitor the directory
	c_code = monitor_directory(
		tcp_client->config.directory,
		on_client_file_created,
		on_client_file_modified,
		on_client_file_deleted,
		on_client_file_renamed
	);
	ERROR_HANDLE_INT_RETURN_INT(c_code, "main(): Failed to monitor the directory\n");

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
	ERROR_HANDLE_INT_RETURN_INT(c_code, "tcp_client_thread(): Invalid parameters, 'arg' should be NULL\n");

	// Variables
	message_t message;

	// Get directory files
	getAllDirectoryFiles();
	pthread_cond_signal(&g_client->zip_file_received);

	// TODO : Receive directory changes from the server
	while (1);

	// Disconnect from the server
	message.type = DISCONNECT;
	message.message = NULL;
	message.size = 0;
	STOUPY_CRYPTO(&message, sizeof(message_t), g_client->config.password);
	c_code = socket_write(g_client->socket, &message, sizeof(message_t)) > 0 ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(c_code, "tcp_client_thread(): Unable to send the message\n");
	INFO_PRINT("tcp_client_thread(): Disconnected from the server\n");

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
	ERROR_HANDLE_INT_RETURN_INT(c_code, "getAllDirectoryFiles(): Unable to send the message\n");

	// Receive the message through the socket
	bytes = socket_read(g_client->socket, &message, sizeof(message_t));
	STOUPY_CRYPTO(&message, sizeof(message_t), g_client->config.password);
	c_code = bytes > 0 ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(c_code, "getAllDirectoryFiles(): Unable to receive the message\n");

	// Check the message type
	c_code = message.type == SEND_ZIP_DIRECTORY ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(c_code, "getAllDirectoryFiles(): Invalid message type received : %d\n", message.type);

	///// Receive the zip file
	// Open the zip file
	FILE *fd = fopen(ZIP_TEMPORARY_FILE, "wb");
	ERROR_HANDLE_PTR_RETURN_INT(fd, "getAllDirectoryFiles(): Unable to open the zip file\n");

	// Receive the zip file
	size_t received_bytes = 0;
	while (received_bytes < message.size) {

		// Read the socket into the c_buffer
		size_t read_size = socket_read(g_client->socket, c_buffer, sizeof(c_buffer));
		STOUPY_CRYPTO(c_buffer, read_size, g_client->config.password);
		c_code = read_size > 0 ? 0 : -1;
		if (c_code == -1) fclose(fd);
		ERROR_HANDLE_INT_RETURN_INT(c_code, "getAllDirectoryFiles(): Unable to receive the zip file\n");

		// Write the c_buffer into the file
		bytes = fwrite(c_buffer, sizeof(byte), read_size, fd);
		c_code = bytes == read_size ? 0 : -1;
		if (c_code == -1) fclose(fd);
		ERROR_HANDLE_INT_RETURN_INT(c_code, "getAllDirectoryFiles(): Unable to write the zip file\n");

		// Update the received bytes
		received_bytes += read_size;
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
	c_code = system(command);
	ERROR_HANDLE_INT_RETURN_INT(c_code, "getAllDirectoryFiles(): Unable to unzip the zip file\n");

	// Remove the zip file
	c_code = remove(ZIP_TEMPORARY_FILE);
	ERROR_HANDLE_INT_RETURN_INT(c_code, "getAllDirectoryFiles(): Unable to remove the zip file\n");

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

	// Variables
	int code = 0;
	int filepath_size = strlen(filepath) + 1;
	int new_filepath_size = new_filepath != NULL ? strlen(new_filepath) + 1 : 0;
	message_t message;
	message.type = action;
	message.message = NULL;
	message.size = 0;
	size_t bytes;

	// Lock the mutex
	pthread_mutex_lock(&g_client->mutex);
	INFO_PRINT("on_client_file_change_handler(): Mutex locked for file '%s'\n", filepath);

	///// Send the message through the socket
	// Copy the first filepath into the c_buffer
	memcpy(c_buffer, filepath, filepath_size);
	STOUPY_CRYPTO(c_buffer, filepath_size, g_client->config.password);

	// Create the message
	message.message = malloc(filepath_size * sizeof(byte));
	ERROR_HANDLE_PTR_RETURN_INT(message.message, "on_client_file_change_handler(): Unable to allocate memory for the message\n");
	memcpy(message.message, c_buffer, filepath_size);
	message.size = filepath_size;

	// Send the message through the socket
	STOUPY_CRYPTO(&message, sizeof(message_t), g_client->config.password);
	bytes = socket_write(g_client->socket, &message, sizeof(message_t));
	code = bytes > 0 ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(code, "on_client_file_change_handler(): Unable to send the message\n");

	// Send the message.message through the socket
	bytes = socket_write(g_client->socket, message.message, message.size);
	code = bytes > 0 ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(code, "on_client_file_change_handler(): Unable to send the message.message\n");

	// Free the message
	free(message.message);
	INFO_PRINT("on_client_file_change_handler(): Message sent\n");

	// Switch case on the action type
	switch (action) {



		// Send the file content when it's created or modified
		case FILE_CREATED:
		case FILE_MODIFIED:
{
	///// Read the file
	// Get the real filepath
	char real_filepath[2048];
	sprintf(real_filepath, "%s%s", g_client->config.directory, filepath);
	INFO_PRINT("on_client_file_change_handler(): Real filepath : '%s'\n", real_filepath);

	// Open the file
	FILE *file = fopen(real_filepath, "rb");
	ERROR_HANDLE_PTR_RETURN_INT(file, "on_client_file_change_handler(): Unable to open the file\n");

	// Send the size of the file
	size_t file_size = 0;
	fseek(file, 0, SEEK_END);
	file_size = ftell(file);
	fseek(file, 0, SEEK_SET);
	size_t file_size_crypted = file_size;
	STOUPY_CRYPTO(&file_size_crypted, sizeof(size_t), g_client->config.password);
	bytes = socket_write(g_client->socket, &file_size_crypted, sizeof(size_t));
	code = bytes > 0 ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(code, "on_client_file_change_handler(): Unable to send the file size\n");
	INFO_PRINT("on_client_file_change_handler(): File size crypted : %zu\n", file_size);

	// Send the file
	size_t bytes_remaining = file_size;
	while (bytes_remaining > 0) {

		// Get the size of the buffer
		size_t buffer_size = CS_BUFFER_SIZE < bytes_remaining ? CS_BUFFER_SIZE : bytes_remaining;
		INFO_PRINT("on_client_file_change_handler(): Bytes remaining : %zu / %zu\n", bytes_remaining, file_size);

		// Read the file into the buffer
		fread(c_buffer, sizeof(byte), buffer_size, file);
		//STOUPY_CRYPTO(c_buffer, buffer_size, g_client->config.password);
		socket_write(g_client->socket, c_buffer, buffer_size);

		// Update the bytes remaining
		bytes_remaining -= buffer_size;
	}

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
	STOUPY_CRYPTO(&new_filepath_size_crypted, sizeof(size_t), g_client->config.password);
	bytes = socket_write(g_client->socket, &new_filepath_size_crypted, sizeof(size_t));
	code = bytes > 0 ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(code, "on_client_file_change_handler(): Unable to send the new filepath size\n");

	// Copy the new filepath into the c_buffer
	memcpy(c_buffer, new_filepath, new_filepath_size);
	STOUPY_CRYPTO(c_buffer, new_filepath_size, g_client->config.password);

	// Send the new filepath through the socket
	bytes = socket_write(g_client->socket, c_buffer, new_filepath_size);
	code = bytes > 0 ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(code, "on_client_file_change_handler(): Unable to send the new filepath\n");
}
			break;
		
		default:
			break;
	}

	// Unlock the mutex
	pthread_mutex_unlock(&g_client->mutex);
	INFO_PRINT("on_client_file_change_handler(): Mutex unlocked\n");

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

