
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
void bytes_encrypter(byte* bytes, size_t size, string_t password) {
	size_t i;
	for (i = 0; i < size && bytes[i] != '\0'; i++) {

		// Remember the original byte
		byte tmp = bytes[i];

		// Encode the byte
		bytes[i] = (bytes[i] ^ password.str[i % password.size]) + (i * 7);

		// Avoid encoding to the '\0' character
		if (bytes[i] == '\0')
			bytes[i] = tmp;
	}
}

/**
 * @brief Decode the message using the password.
 * 
 * @param bytes The message to decode.
 * @param size The size of the message.
 * @param password The password to use for decoding.
 * 
 * @return void
 */
void bytes_decrypter(byte* bytes, size_t size, string_t password) {
	size_t i;
	for (i = 0; i < size && bytes[i] != '\0'; i++) {

		// Remember the original byte
		byte tmp = bytes[i];

		// Decode the byte
		bytes[i] = (bytes[i] - (i * 7)) ^ password.str[i % password.size];

		// Avoid decoding to the '\0' character
		if (bytes[i] == '\0')
			bytes[i] = tmp;
	}
}
