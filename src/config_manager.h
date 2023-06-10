
#ifndef __CONFIG_MANAGER_H__
#define __CONFIG_MANAGER_H__

#include "universal_utils.h"

#define CONFIG_FILE "config.ini"
#define CONFIG_FILE_IN_BIN "bin/config.ini"

// Structure of the configuration file
typedef struct {
	char directory[1024];
	simple_string_t password;
	char ip[16];
	int port;
} config_t;

// Function Prototypes
config_t read_config_file();

#endif

