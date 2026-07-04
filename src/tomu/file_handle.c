#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>

#include "errors.h"
#include "structs.h"
#include "file_handle.h"
#include "macros.h"
#include "streaming.h"

static const char *filter_fmts[] = {
    ".mp3", ".opus", ".flac", ".aac", 
    ".ogg", ".m4a", ".wav", ".wma"  
};

// add src (file/url) to list queue
static void add_to_list(LIST_FILES *queue, const char *src)
{
  pthread_mutex_lock(&tctx.list.lock);

  queue->queue_lists = realloc(queue->queue_lists, sizeof(char *) * (queue->queue_count + 1)); // make space
  queue->queue_lists[queue->queue_count] = strdup(src); // add this into list
  queue->queue_count++;

  pthread_cond_signal(&tctx.list.item_ready);
  pthread_mutex_unlock(&tctx.list.lock);
}

// [src= (file)|(url)] , [type= (0=file)|(1=url)]
void queue_add(const char *src, SRC_TYPE src_type)
{
  if (src_type == SRC_FILE_RAW || src_type == SRC_FILE_DIR) {
    printf("file file file or dir\n");
    path_handle(src, &tctx.list);
  }
  else if (src_type == SRC_URL_PLAYLIST) { // check if playlist
    extract_playlist_url(src);
  }
  else if (src_type == SRC_URL_RAW) {
    add_to_list(&tctx.list, src);
  }
}

// ---------------------------------------------------------------------

// check type of src [(file/dir)|(url/url playlist)]
SRC_TYPE checking_src_type(const char *src)
{
  if (IS_PATH_DIR(src))     return SRC_FILE_DIR;
  if (IS_PATH_RAW(src))     return SRC_FILE_RAW;
  if (IS_URL_PLAYLIST(src)) return SRC_URL_PLAYLIST;
  if (IS_URL_RAW(src))      return SRC_URL_RAW;
  return SRC_NONE;
}

// checking from src 
int handle_src(const char *src)
{
  SRC_TYPE src_type = checking_src_type(src);
  switch (src_type)
  {
    case SRC_NONE: return -1;
    case SRC_FILE_DIR: case SRC_FILE_RAW: path_handle(src, &tctx.list); break;
    case SRC_URL_PLAYLIST: extract_playlist_url(src); break;
    case SRC_URL_RAW: add_to_list(&tctx.list, src); break;
  }
  return 1;
}

/*
// this function for checking from an audio file format
// return 1-> is an audio file, 0 -> file is not an audio file
*/
int is_audio_fmt(const char *path)
{
  // 1. pointer to '.' from path
  const char *point = strrchr(path, '.'); // start from right to left to find '.'
  if (!point) return 0; // if not there '.' skip

  // 2. check from format

  // is audio file!
  for_each_arr(filter_fmts) if (!strcmp(point, filter_fmts[i])) return 1;

  // is not audio file!
  warn("tomu: is not audio file '%s'\n", path);
  return 0;
}

/*
 * this fn about handle audio file/dir only 
 * if get dir will scanning and get all files
 * if get file will push it into the queue list
*/
void path_handle(const char *path, LIST_FILES *queue)
{
  usleep(4);
  struct stat st;
  if (stat(path, &st) < 0) {
      warn("%s", path);
      return;
  }

  printf("is here file\n");
  if (S_ISREG(st.st_mode)) { //file
      if (!tctx.list.filter_files || is_audio_fmt(path)) {
        add_to_list(queue, path);
      }
  }
  else if (S_ISDIR(st.st_mode)) { //dir
      struct dirent *file;
      DIR *dir_path = opendir(path);
      if (!dir_path) die("Cannot open directory: %s", path);

      while ((file = readdir(dir_path)) != NULL) {
          if (!strcmp(file->d_name, ".") || !strcmp(file->d_name, "..")) continue;

          char buf[1024];
          snprintf(buf, sizeof(buf), "%s/%s", path, file->d_name);
          path_handle(buf, queue);
      }
      closedir(dir_path);
  }
}
