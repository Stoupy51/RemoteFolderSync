
#include "config_manager.h"
#include "utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>

/**
 * @brief Function that reads the config file and returns the config_t structure.
 * 
 * @return config_t The configuration read from the file.
 */
config_t read_config_file() {

	// Variables
	config_t config;
	memset(&config, 0, sizeof(config_t));
	char *line = NULL;
	int code;

	// Try to open the file
	int fd = open(CONFIG_FILE, O_RDONLY);
	if (fd == -1) {
		
		// Try to open the file in the bin folder
		fd = open(CONFIG_FILE_IN_BIN, O_RDONLY);
		if (fd < 0) {
			ERROR_PRINT("read_config_file(): Unable to open the config file.\n");
			return config;
		}
	}

	// Read the file line by line
	while (get_line_from_file(&line, fd) != -1) {

		// Check if the line is empty
		if (line[0] == '\n' || line[0] == '\0')
			continue;

		// Get the key and the value
		char *key = strtok(line, "=");
		char *value = strtok(NULL, "=");

		// Remove the \n at the end of the value if there is one
		int len = strlen(value) - 1;
		if (value[len] == '\n')
			value[len] = '\0';

		///// Check the key
		// Check if the key is directory
		if (strcmp(key, "directory") == 0) {
			strcpy(config.directory, value);
		}

		// Check if the key is password
		else if (strcmp(key, "password") == 0) {
			strcpy(config.password, value);
		}

		// Check if the key is ip
		else if (strcmp(key, "ip") == 0) {
			strcpy(config.ip, value);
		}

		// Check if the key is port
		else if (strcmp(key, "port") == 0) {
			config.port = atoi(value);
		}
	}

	// Free the line
	free(line);

	// Close the file
	code = close(fd);
	if (code < 0)
		ERROR_PRINT("read_config_file(): Unable to close the config file.\n");
	
	// Return the config
	return config;
}

