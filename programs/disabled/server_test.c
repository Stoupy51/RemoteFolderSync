
#include <stdlib.h>

#include "../src/global_utils.h"
#include "../src/file_watcher.h"
#include "../src/config_manager.h"
#include "../src/server/s_tcp_manager.h"

int code;
char key;
config_t config;

/**
 * @brief Function run at the end of the program
 * [registered with atexit()] in the main() function.
 * 
 * @return void
 */
void exitProgram() {

	// Print end of program
	INFO_PRINT("exitProgram(): End of program, press enter to exit\n");
	getchar();
	exit(0);
}

/**
 * This program is an introduction to file watching using Windows or Linux.
 * It creates a TCP server that monitors a directory, and accepts connections from clients.
 * The configuration of the server is taken from the file "config.ini".
 * 
 * @author Stoupy51 (COLLIGNON Alexandre)
 */
int main() {

	// Print program header and register exitProgram() with atexit()
	mainInit("main(): Server test program\n");
	atexit(exitProgram);

	// Read configuration file
	config = read_config_file();
	code = config.port == 0 ? -1 : 0;
	ERROR_HANDLE_INT_RETURN_INT(code, "main(): Unable to read the configuration file\n");
	INFO_PRINT("main(): Configuration file read\n");

	// Setup the TCP server
	tcp_server_t tcp_server;
	code = setup_tcp_server(config, &tcp_server);
	ERROR_HANDLE_INT_RETURN_INT(code, "main(): Unable to setup the TCP server\n");

	// Wait for the server to finish
	tcp_server_run(&tcp_server);

	// Final print and return
	INFO_PRINT("main(): End of program\n");
	return 0;
}


