#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>

#include "CLIENT_DATA.h"
#include "control.h"
#include "../shared/share_utils1.h"
#include "../shared/shared_control.h"

static const char *filter_fmt[] = {
    ".mp3", ".opus", ".flac", ".aac", 
    ".ogg", ".m4a", ".wav", ".wma"  
};

void queue_free(PlaybackQueue *queue)
{
  if (!queue->dir.files) return;

  for_each_num(queue->dir.count) {
    free(queue->dir.files[i]);
  }

  
  free(queue->dir.files);
  queue->dir.files = NULL;
}

// // Read all the files in dir and return them 
// void extractDir(const char* path, Dir_File *dir)
// {
//
// clean_files:
//   closedir(dir_path);
//   for (int i = 0; i < count_files; i++)
//       free(files[i]);
//   free(files);
//   warn("Not enough memory for files!");
//   dir->totalFiles = 0;
//   dir->files = NULL;
//   die("extractDir:");
// }
/*
 * Check if file extension matches one of the supported audio formats.
 *
 * Returns:
 *   1 -> file is an audio file
 *   0 -> file is not an audio file
 */
int is_audio_fmt(const char *path)
{
  // 1. pointer to '.' from path
  const char *point = strrchr(path, '.');
  if (!point) return 0; // if not there '.' skip

  // 2. check from format

  // is audio file!
  for_each_arr(filter_fmt) if (!strcmp(point, filter_fmt[i])) return 1;

  // is not audio file!
  printf("tomucli: is not audio file '%s'\n", path);
  return 0;

}

void path_handle(const char *path, PlaybackQueue *queue)
{
  usleep(4);
  struct stat st; 
  if (stat(path, &st) < 0) {
    warn("%s", path);
    return;
  }
  
  // is file
  if (S_ISREG(st.st_mode)) {
    if (!ctx.filter_files || is_audio_fmt(path)) {
      // Allocate space for one more file
      queue->dir.files = realloc(queue->dir.files, sizeof(char *) * (queue->dir.count + 1));
      queue->dir.files[queue->dir.count] = strdup(path);
      queue->dir.count++;
    }
  }

   // is dir
  else if (S_ISDIR(st.st_mode)) {
    // read dir and extract files
    struct dirent* file;
    DIR *dir_path = opendir(path);
    if (!dir_path) die("Cannot open directory: %s", path);

    while ((file = readdir(dir_path)) != NULL) { // read each each files from this dir
      if ((!strcmp(file->d_name, ".")) || (!strcmp(file->d_name, ".."))) continue; // skip ., .. dir

      // read depth dir
      char buf[1024];
      snprintf(buf, sizeof(buf), "%s/%s", path, file->d_name);
      printf("%s\n", buf);
      path_handle(buf, queue);
    }

    closedir(dir_path);
  } 
}
