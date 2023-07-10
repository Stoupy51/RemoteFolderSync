
#include "file_watcher.h"
#include "universal_utils.h"

#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef _WIN32

#include <windows.h>

/**
 * @brief Monitor a directory for file creation, modification and deletion
 * 
 * @param directory_path	Path to the directory to monitor
 * @param file_created		Function to call when a file is created
 * @param file_modified		Function to call when a file is modified
 * @param file_deleted		Function to call when a file is deleted
 * @param file_renamed		Function to call when a file is renamed
 * 
 * @return int				0 if success, -1 otherwise
 */
int monitor_directory(const char *directory_path, file_created_handler file_created, file_modified_handler file_modified, file_deleted_handler file_deleted, file_renamed_handler file_renamed) {

	// Error code handler
	int code;

	// Create the directory handle
	HANDLE directory_handle = CreateFile(
		directory_path,
		FILE_LIST_DIRECTORY,
		FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
		NULL,
		OPEN_EXISTING,
		FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED,
		NULL
	);

	// Check for errors
	code = (directory_handle == INVALID_HANDLE_VALUE) ? -1 : 0;
	ERROR_HANDLE_INT_RETURN_INT(code, "monitor_directory(): Failed to open the directory");

	// Print the directory path
	INFO_PRINT("Monitoring directory: %s\n", directory_path);

	// Preparations
	byte buffer[1024];
	DWORD bytesReturned;
	OVERLAPPED overlapped = {0};

	// Filepath buffers
	char filepath_new[MAX_PATH];
	char filepath_old[MAX_PATH];
	char filepath_new_full[MAX_PATH];

	// Read the events (File creation, modification, deletion and renaming, file deletion in subfolders)
	while (ReadDirectoryChangesW(
		directory_handle,
		buffer,
		sizeof(buffer),
		TRUE,
		FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_ATTRIBUTES | FILE_NOTIFY_CHANGE_SIZE | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_SECURITY,
		&bytesReturned,
		&overlapped,
		NULL
	)) {

		if (GetOverlappedResult(directory_handle, &overlapped, &bytesReturned, TRUE) == 0) {
			WARNING_PRINT("monitor_directory(): GetOverlappedResult() failed\n");
			continue;
		}

		// Get the notify information
		FILE_NOTIFY_INFORMATION *notifyInfo = (FILE_NOTIFY_INFORMATION *)buffer;

		// For each event
		do {

			// Get the filepath
			int filepath_length = WideCharToMultiByte(CP_UTF8, 0, notifyInfo->FileName, notifyInfo->FileNameLength / sizeof(WCHAR), filepath_new, MAX_PATH, NULL, NULL);
			filepath_new[filepath_length] = '\0';

			// Replace backslashes with slashes
			int i;
			for (i = 0; i < filepath_length; i++)
				if (filepath_new[i] == '\\')
					filepath_new[i] = '/';
			
			// Get the full filepath
			sprintf(filepath_new_full, "%s%s", directory_path, filepath_new);

			// If the detected event is about a folder, skip it
			struct stat path_stat;
			stat(filepath_new_full, &path_stat);
			if (S_ISDIR(path_stat.st_mode)) {
				WARNING_PRINT("monitor_directory(): Skipping event about folder: '%s'\n", filepath_new_full);
			}

			// Call the appropriate function
			else switch (notifyInfo->Action) {

				case FILE_ACTION_ADDED:
					code = file_created(filepath_new);
					ERROR_HANDLE_INT_RETURN_INT(code, "monitor_directory(): Error when calling file_created() function\n");
					break;

				case FILE_ACTION_MODIFIED:
					code = file_modified(filepath_new);
					ERROR_HANDLE_INT_RETURN_INT(code, "monitor_directory(): Error when calling file_modified() function\n");
					break;

				case FILE_ACTION_REMOVED:
					code = file_deleted(filepath_new);
					ERROR_HANDLE_INT_RETURN_INT(code, "monitor_directory(): Error when calling file_deleted() function\n");
					break;
				
				case FILE_ACTION_RENAMED_OLD_NAME:
					strcpy(filepath_old, filepath_new);
					break;
				
				case FILE_ACTION_RENAMED_NEW_NAME:
					code = file_renamed(filepath_old, filepath_new);
					ERROR_HANDLE_INT_RETURN_INT(code, "monitor_directory(): Error when calling file_renamed() function\n");
					break;
				
				default:
					WARNING_PRINT("monitor_directory(): Unknown action: %ld\n", notifyInfo->Action);
					break;
			}

			// Move to the next event
			notifyInfo = (FILE_NOTIFY_INFORMATION*)((BYTE*)notifyInfo + notifyInfo->NextEntryOffset);
		}
		while (notifyInfo->NextEntryOffset != 0);
	}

	// Close the directory handle
	code = CloseHandle(directory_handle) ? 0 : -1;
	ERROR_HANDLE_INT_RETURN_INT(code, "monitor_directory(): Cannot close directory handle\n");

	// Return success
	return 0;
}


#else

#include <sys/inotify.h>

#define WATCH_EVENT_SIZE (sizeof(struct inotify_event))
#define WATCH_BUFFER_SIZE (1024 * (WATCH_EVENT_SIZE + 16))

/**
 * @brief Monitor a directory for file creation, modification and deletion
 * 
 * @param directory_path	Path to the directory to monitor
 * @param file_created		Function to call when a file is created
 * @param file_modified		Function to call when a file is modified
 * @param file_deleted		Function to call when a file is deleted
 * @param file_renamed		Function to call when a file is renamed
 * 
 * @return int				0 if success, -1 otherwise
 */
int monitor_directory(const char *directory_path, file_created_handler file_created, file_modified_handler file_modified, file_deleted_handler file_deleted, file_renamed_handler file_renamed) {

	// Error code handler
	int code;

	// Create the inotify instance
	int fd = inotify_init();
	ERROR_HANDLE_INT_RETURN_INT(fd, "monitor_directory(): Cannot create inotify instance\n");

	// Add the directory to the watch list
	int wd = inotify_add_watch(fd, directory_path, IN_ALL_EVENTS);
	ERROR_HANDLE_INT_RETURN_INT(wd, "monitor_directory(): Cannot add directory to the watch list\n");

	// Print the directory path
	INFO_PRINT("Monitoring directory: %s\n", directory_path);

	// Prepare the buffer
	byte buffer[WATCH_BUFFER_SIZE];
	size_t bytesRead;

	// Read the events
	while ((bytesRead = read(fd, buffer, WATCH_BUFFER_SIZE)) > 0) {

		// For each event in the buffer (there can be multiple events in the buffer)
		byte *ptr = buffer;
		while (ptr < (buffer + bytesRead)) {

			// Get the event from the buffer
			struct inotify_event *event = (struct inotify_event *)ptr;

			// If the event is valid
			if (event->len > 0) {

				///// Call the appropriate handler depending on the event type
				// If the file was created
				if (event->mask & IN_CREATE) {
					code = file_created(event->name);
					ERROR_HANDLE_INT_RETURN_INT(code, "monitor_directory(): Error in file_created_handler\n");
				}

				// If the file was modified
				if (event->mask & IN_MODIFY) {
					code = file_modified(event->name);
					ERROR_HANDLE_INT_RETURN_INT(code, "monitor_directory(): Error in file_modified_handler\n");
				}

				// If the file was deleted
				if (event->mask & IN_DELETE) {
					code = file_deleted(event->name);
					ERROR_HANDLE_INT_RETURN_INT(code, "monitor_directory(): Error in file_deleted_handler\n");
				}

				// If the file was renamed
				if (event->mask & IN_MOVED_FROM) {
					code = file_renamed(event->name, NULL);
					ERROR_HANDLE_INT_RETURN_INT(code, "monitor_directory(): Error in file_renamed_handler\n");
				}
			}

			// Move to the next event
			ptr += WATCH_EVENT_SIZE + event->len;
		}
	}

	// Remove the directory from the watch list
	code = inotify_rm_watch(fd, wd);
	close(fd);
	ERROR_HANDLE_INT_RETURN_INT(code, "monitor_directory(): Cannot remove directory from the watch list\n");

	// Return success
	return 0;
}

#endif

