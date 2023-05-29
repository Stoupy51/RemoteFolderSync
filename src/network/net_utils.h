
#ifndef __NETWORK_UTILS_H__
#define __NETWORK_UTILS_H__

#include "../utils.h"

#define ZIP_TEMPORARY_FILE "remote_folder_sync_temp.zip"


// Message types
typedef enum message_type_t {
	GET_ZIP_DIRECTORY = 1,
	SEND_ZIP_DIRECTORY = 2,

	DISCONNECT = 100,
} message_type_t;

// Structure for exchanging messages between client and server
typedef struct {

	// Message type
	message_type_t type;

	// Message (can be null)
	byte* message;
	size_t size;

} message_t;

// Functions prototypes
void message_coder_decoder(byte* message, size_t size, char* password);

#endif

