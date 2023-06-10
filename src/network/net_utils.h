
#ifndef __NETWORK_UTILS_H__
#define __NETWORK_UTILS_H__

#include "../universal_utils.h"
#include "../universal_socket.h"

#define ZIP_TEMPORARY_FILE "remote_folder_sync_temp.zip"
#define CS_BUFFER_SIZE 1024 * 1024		// 1 MB


// Message types
typedef enum message_type_t {

	FILE_CREATED = 10,
	FILE_MODIFIED = 11,
	FILE_DELETED = 12,
	FILE_RENAMED = 13,

	DISCONNECT = 100,
	VALID_RESPONSE = 61166,

} message_type_t;

// Structure for client information
typedef struct client_info_t {
	SOCKET socket;
	struct sockaddr_in address;
	char *ip;
	int port;
} client_info_t;

// Structure for exchanging messages between client and server
typedef struct {

	// Message type
	message_type_t type;

	// Message (can be null)
	byte* message;
	size_t size;

} message_t;

// Functions prototypes
void bytes_encrypter(byte* bytes, size_t size, simple_string_t password);
void bytes_decrypter(byte* bytes, size_t size, simple_string_t password);
#define ENCRYPT_BYTES(bytes, size, password) bytes_encrypter((byte*)bytes, size, password)
#define DECRYPT_BYTES(bytes, size, password) bytes_decrypter((byte*)bytes, size, password)

#endif

