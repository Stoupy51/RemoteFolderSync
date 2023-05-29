
#include "net_utils.h"
#include "../config_manager.h"

/**
 * @brief Encode the message using the password.
 * 
 * @param message The message to encode.
 * @param size The size of the message.
 * @param password The password to use for encoding.
 * 
 * @return void
 */
void message_coder_decoder(byte* message, size_t size, byte* password) {
	size_t i;
	for (i = 0; i < size; i++)
		message[i] ^= password[i % PASSWORD_SIZE];
}

