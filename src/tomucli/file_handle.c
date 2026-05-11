#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>

#include "CLIENT_DATA.h"
#include "control.h"
#include "../shared/share_utils.h"
#include "../shared/shared_control.h"


void queue_free(PlaybackQueue *queue)
{
  if (!queue->dir.files) return;

  for_each_num(queue->dir.totalFiles) {
    free(queue->dir.files[i]);
  }

  
  free(queue->dir.files);
  queue->dir.files = NULL;
}

// Read all the files in dir and return them 
void extractDir(const char* path, Dir_File *dir)
{
  int count_files = 0;
  char **files = NULL;

  // read dir and extract files
  struct dirent* file;
  DIR *dir_path = opendir(path);
  if (!dir_path) die("Cannot open directory: %s", path);

  while ((file = readdir(dir_path)) != NULL) { // read each each files from this dir
      if ((!strcmp(file->d_name, ".")) || (!strcmp(file->d_name, ".."))) continue; // skip ., .. dir

      // Grow array if needed
      if (!(files = realloc(files, sizeof(char*) * (count_files + 1)))) goto clean_files; // add new size malloc

      files[count_files] = strdup(file->d_name); // save this file to array
      if (!files[count_files]) goto clean_files;
      count_files++;
  }

  dir->totalFiles = count_files;
  dir->files = files;
  closedir(dir_path);
  return;

clean_files:
  closedir(dir_path);
  for (int i = 0; i < count_files; i++)
      free(files[i]);
  free(files);
  warn("Not enough memory for files!");
  dir->totalFiles = 0;
  dir->files = NULL;
  die("extractDir:");
}

void path_handle(int server_fd, const char *path, PlaybackQueue *queue)
{
    struct stat st; 
    if (stat(path, &st) < 0) die("file:");
    
    if (S_ISDIR(st.st_mode)) {
        strncpy(queue->dir.base_path, path, sizeof(queue->dir.base_path) - 1);
        extractDir(path, &queue->dir);
        queue->has_queue = 1;  // IMPORTANT: Set this flag
        queue->current_index = queue->dir.rand_num % queue->dir.totalFiles;

    } else if (S_ISREG(st.st_mode)) {
      send_path(ctx.server_fd, path);
      // queue->dir.files = malloc(sizeof(char *));
      // queue->dir.files[0] = strdup(path);
      // queue->dir.totalFiles = 1;
      // queue->has_queue = 1;  // Single file mode

    } else die("File:");
}
