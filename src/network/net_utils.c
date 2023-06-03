
#include "net_utils.h"
#include "../config_manager.h"

/**
 * @brief Encode the message using the password.
 * 
 * @param bytes The message to encode.
 * @param size The size of the message.
 * @param password The password to use for encoding.
 * 
 * @return void
 */
void message_coder_decoder(byte* bytes, size_t size, string_t password) {
	size_t i;
	for (i = 0; i < size && bytes[i] != '\0'; i++)
		bytes[i] ^= password.str[i % password.size];
}

