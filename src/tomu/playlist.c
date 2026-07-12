#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>

#include "errors.h"
#include "structs.h"
#include "playlist.h"
#include "macros.h"
#include "stream.h"

static const char *filter_fmts[] = {
  ".mp3", ".opus", ".flac", ".aac",
  ".ogg", ".m4a", ".wav", ".wma"
};

static void add_to_list(LIST_FILES *queue, const char *src)
{
  pthread_mutex_lock(&tctx.list.pt_lock);

  char **tmp = realloc(queue->queue_lists, sizeof(char *) * (queue->queue_count + 1));
  if (!tmp) { pthread_mutex_unlock(&tctx.list.pt_lock); return; }
  queue->queue_lists = tmp;
  queue->queue_lists[queue->queue_count] = strdup(src);
  queue->queue_count++;

  pthread_cond_signal(&tctx.list.pt_signal);
  pthread_mutex_unlock(&tctx.list.pt_lock);
}

SRC_TYPE extract_src_type(const char *src)
{
  if (IS_PATH_RAW(src))     return SRC_FILE_RAW;
  else if (IS_URL_RAW(src)) return SRC_URL_RAW;
  else return               SRC_NONE;
}

int src_handle(const char *src)
{
  // step 1 check format
  SRC_TYPE src_type;

  if      (IS_PATH_DIR(src))  src_type = SRC_FILE_DIR;
  else if (IS_PATH_RAW(src))  src_type = SRC_FILE_RAW;
  else if (IS_URL_RAW(src))   src_type = IS_URL_PLAYLIST(src) ? SRC_URL_PLAYLIST : SRC_URL_RAW;
  else src_type = SRC_NONE;


  // step 2 run correct handle
  switch (src_type) {
    case SRC_NONE:         return -1;
    case SRC_FILE_DIR: case SRC_FILE_RAW:     path_handle(src, &tctx.list); break;
    case SRC_URL_PLAYLIST: extract_playlist_url(src); break;
    case SRC_URL_RAW:      add_to_list(&tctx.list, src); break;
  }
  tctx.list.src_type = src_type;
  return 1;
}

static int is_audio_fmt(const char *path)
{
  const char *point = strrchr(path, '.');
  if (!point) return 0;

  for_each_arr(filter_fmts)
    if (!strcmp(point, filter_fmts[i])) return 1;

  return 0;
}

void path_handle(const char *path, LIST_FILES *queue)
{
  struct stat st;
  if (stat(path, &st) < 0) {
    warn("%s", path);
    return;
  }

  if (S_ISREG(st.st_mode)) {
    if (!tctx.list.filter_files || is_audio_fmt(path))
      add_to_list(queue, path);
  } else if (S_ISDIR(st.st_mode)) {
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
