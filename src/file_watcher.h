
#ifndef __FILE_WATCHER_H__
#define __FILE_WATCHER_H__

typedef int (*file_action_handler)(const char *filepath);
typedef int (*file_renamed_handler)(const char *filepath_old, const char *filepath_new);
typedef file_action_handler file_created_handler;
typedef file_action_handler file_modified_handler;
typedef file_action_handler file_deleted_handler;

int monitor_directory(const char *directory_path, file_created_handler file_created, file_modified_handler file_modified, file_deleted_handler file_deleted, file_renamed_handler file_renamed);

#endif

