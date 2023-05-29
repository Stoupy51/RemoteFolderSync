
#ifndef __CONFIG_MANAGER_H__
#define __CONFIG_MANAGER_H__

#include "utils.h"

#define CONFIG_FILE "config.ini"
#define CONFIG_FILE_IN_BIN "bin/config.ini"
#define PASSWORD_SIZE 256

// Structure of the configuration file
typedef struct {
	char directory[1024];
	char password[PASSWORD_SIZE];
	char ip[16];
	int port;
} config_t;

// Function Prototypes
config_t read_config_file();

#endif

