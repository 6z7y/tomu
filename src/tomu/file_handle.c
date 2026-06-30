#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdarg.h>

#include "DATA.h"
#include "control.h"
#include "streaming.h"
#include "utils1.h"
#include "utils2.h"

static const char *filter_fmt[] = {
    ".mp3", ".opus", ".flac", ".aac", 
    ".ogg", ".m4a", ".wav", ".wma"  
};

// void queue_free(PlaybackQueue *queue)
// {
//   if (!queue->dir.files) return;
//
//   for_each_num(queue->dir.count) {
//     free(queue->dir.files[i]);
//   }
//
//
//   free(queue->dir.files);
//   queue->dir.files = NULL;
// }

// Read all the files in dir and return them 
// void extractDir(const char* path, LIST_FILES *dir)
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

void path_handle(const char *path, LIST_FILES *queue)
{
    usleep(4);
    struct stat st;
    if (stat(path, &st) < 0) {
        warn("%s", path);
        return;
    }

    if (S_ISREG(st.st_mode)) {
        if (!tctx.list.filter_files || is_audio_fmt(path)) {
            queue->queue_lists = realloc(queue->queue_lists,
                sizeof(char *) * (queue->queue_count + 1));
            queue->queue_lists[queue->queue_count] = strdup(path);
            queue->queue_count++;
        }
    }
    else if (S_ISDIR(st.st_mode)) {
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

void queue_add(const char *path)
{
    int is_url_path = strncmp(path, "http://", 7) == 0 ||
                      strncmp(path, "https://", 8) == 0;

    if (is_url_path && strstr(path, "list=")) {
        // it's a playlist — expand it (each entry calls queue_add recursively)
        resolve_playlist(path);
        return;
    }

    pthread_mutex_lock(&tctx.list.lock);

    if (is_url_path) {
        // check: don't add the same URL twice
        // for (int i = 0; i < tctx.list.queue_count; i++) {
        //     if (strcmp(tctx.list.queue_lists[i], path) == 0) {
        //         pthread_mutex_unlock(&tctx.list.lock);
        //         printf("[queue] already in list: %s\n", path);
        //         return;
        //     }
        // }
        tctx.list.queue_lists = realloc(tctx.list.queue_lists,
            sizeof(char *) * (tctx.list.queue_count + 1));
        tctx.list.queue_lists[tctx.list.queue_count] = strdup(path);
        tctx.list.queue_count++;
    } else {
        path_handle(path, &tctx.list);
    }

    pthread_cond_signal(&tctx.list.item_ready);
    pthread_mutex_unlock(&tctx.list.lock);
}
