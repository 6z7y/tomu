#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <stdarg.h>

#include "share_data.h"
#include "share_backend.h"
#include "../shared/shared_control.h"

// Send a file/dir path to server for playback
void send_path(int server_fd, const char *path)
{
    Command cmd = CMD_PATH;
    int pathlen = strlen(path);

    // send: CMD_PATH | length | path
    write(server_fd, &cmd,     sizeof(Command));
    write(server_fd, &pathlen, sizeof(int));
    write(server_fd, path,     pathlen);
}

// Read all the files in dir and return them 
void extractDir(const char* path, Dir_File *dir)
{
    struct dirent* file;
    DIR *dir_path = opendir(path);
    int count_files = 0;
    char **files = NULL;

    if (!dir_path) die("Cannot open directory: %s", path);

    while ((file = readdir(dir_path)) != NULL) {
        if ((!strcmp(file->d_name, ".")) || (!strcmp(file->d_name, ".."))) continue;

        // Grow array if needed
        files = realloc(files, sizeof(char*) * (count_files + 1));
        if (!files) goto clean_files;

        files[count_files] = strdup(file->d_name);
        count_files++;
    }

    closedir(dir_path);
    dir->totalFiles = count_files;
    dir->files = files;
    return;

clean_files:
    closedir(dir_path);
    for (int i = 0; i < count_files; i++)
        free(files[i]);
    free(files);
    warn("Not enough memory for files!");
    dir->totalFiles = 0;
    dir->files = NULL;
}

// Send the next file from queue
void send_next_from_queue(int server_fd, PlaybackQueue *queue)
{
    if (!queue->has_queue) return;
    
    char filename[1024];
    snprintf(filename, sizeof(filename), "%s/%s", 
             queue->base_path, queue->dir.files[queue->dir.currentFile]);
    
    send_path(server_fd, filename);
    // printf("\nNow playing [%d/%d]: %s\n", 
    //        queue->dir.currentFile + 1, 
    //        queue->dir.totalFiles,
    //        queue->dir.files[queue->dir.currentFile]);
}

// Handle playback completion
void handle_playback_complete(int server_fd, PlaybackQueue *queue)
{
    if (!queue->has_queue || queue->dir.totalFiles == 0) return;
    
    // Move to next file (with wrap-around)
    queue->dir.currentFile = (queue->dir.currentFile + 1) % queue->dir.totalFiles;
    
    // Send the next file
    send_next_from_queue(server_fd, queue);
}

void path_handle(int server_fd, const char *path, PlaybackQueue *queue)
{
    struct stat st; // checker dir/file
    if (stat(path, &st) < 0) die("file:");

    // If path is a directory
    if (S_ISDIR(st.st_mode)) {
        // Store the directory info in the queue
        extractDir(path, &queue->dir);
        strncpy(queue->base_path, path, sizeof(queue->base_path) - 1);
        queue->base_path[sizeof(queue->base_path) - 1] = '\0';
        queue->has_queue = 1;

        
        if (queue->dir.totalFiles == 0) {
            warn("No files in directory: %s", path);
            queue->has_queue = 0;
            return;
        }
        
        // Start with a random file
        srand(time(NULL));
        queue->dir.currentFile = rand() % queue->dir.totalFiles;
        
        printf("Playing: %s\n", path); // print name file
        // Send the first file
        send_next_from_queue(server_fd, queue);
    }
    // If path is a regular file
    else if (S_ISREG(st.st_mode)) {
        // Single file mode - clear any queue
        if (queue->has_queue) {
            for (int i = 0; i < queue->dir.totalFiles; i++)
                free(queue->dir.files[i]);
            free(queue->dir.files);
            queue->has_queue = 0;
        }
        send_path(server_fd, path);
        printf("Playing: %s\n", path); // print name file
    }
    else warn("File:");
}

