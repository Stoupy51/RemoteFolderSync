
#include <stdlib.h>

#include "../src/utils.h"
#include "../src/file_watcher.h"

int code;
char key;

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
 * @brief Function called when a file is created.
 * 
 * @param filepath	Path of the file created
 * 
 * @return int	0 if success, -1 otherwise
 */
int on_file_created(const char *filepath) {
	INFO_PRINT("file_created_handler(): File '%s' created\n", filepath);
	return 0;
}

/**
 * @brief Function called when a file is modified.
 * 
 * @param filepath	Path of the file modified
 * 
 * @return int	0 if success, -1 otherwise
 */
int on_file_modified(const char *filepath) {
	INFO_PRINT("file_modified_handler(): File '%s' modified\n", filepath);
	return 0;
}

/**
 * @brief Function called when a file is deleted.
 * 
 * @param filepath	Path of the file deleted
 * 
 * @return int	0 if success, -1 otherwise
 */
int on_file_deleted(const char *filepath) {
	INFO_PRINT("file_deleted_handler(): File '%s' deleted\n", filepath);
	return 0;
}

/**
 * @brief Function called when a file is renamed.
 * 
 * @param filepath_old	Old path of the file
 * @param filepath_new	New path of the file
 * 
 * @return int	0 if success, -1 otherwise
 */
int on_file_renamed(const char *filepath_old, const char *filepath_new) {
	INFO_PRINT("file_renamed_handler(): File '%s' renamed to '%s'\n", filepath_old, filepath_new);
	return 0;
}

/**
 * This program is an introduction to file watching using Windows or Linux.
 * 
 * @param argc	Number of arguments
 * @param argv	List of arguments
 * 
 * @author Stoupy51 (COLLIGNON Alexandre)
 */
int main(int argc, char **argv) {

	// Variables
	char *directory_path;

	// Check for arguments
	if (argc != 2) {
		INFO_PRINT("main(): No directory path provided, using current directory\n");
		directory_path = ".";
	}
	else
		directory_path = argv[1];

	// Print program header and register exitProgram() with atexit()
	mainInit("main(): File Watching test program\n");
	atexit(exitProgram);

	// Monitor the directory
	code = monitor_directory(directory_path, on_file_created, on_file_modified, on_file_deleted, on_file_renamed);
	ERROR_HANDLE_INT_RETURN_INT(code, "main(): Failed to monitor the directory\n");

	// Final print and return
	INFO_PRINT("main(): End of program\n");
	return 0;
}


