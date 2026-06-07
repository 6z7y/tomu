#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include "backend.h"
#include "../shared/share_utils2.h"
#include "CLIENT_DATA.h"


// DELETE these 4 lines from backend.c:
static const int  *bar_width = &ctx.cfg.progress.width;
static const char *done      = ctx.cfg.progress.done;
static const char *current   = ctx.cfg.progress.current;
static const char *remaining = ctx.cfg.progress.remaining;
static const char *color     = ctx.cfg.progress.color;

void parse_json(const char *key)
{
  TomuStatus *s = &ctx.status;
  
  // Parse status section
  if (strstr(key, "\"duration\"")) {
      char *p = strstr(key, "\"duration\"");
      p = strchr(p, ':');
      p++;
      s->duration = atoi(p);
  }
  
  if (strstr(key, "\"position\"")) {
      char *p = strstr(key, "\"position\"");
      p = strchr(p, ':');
      p++;
      s->position = atoi(p);
  }
  
  if (strstr(key, "\"paused\"")) {
      char *p = strstr(key, "\"paused\"");
      p = strchr(p, ':');
      p++;
      s->paused = atoi(p);
  }
  
  if (strstr(key, "\"volume\"")) {
      char *p = strstr(key, "\"volume\"");
      p = strchr(p, ':');
      p++;
      s->volume = atof(p);
  }
  
  if (strstr(key, "\"speed\"")) {
      char *p = strstr(key, "\"speed\"");
      p = strchr(p, ':');
      p++;
      s->speed = atof(p);
  }
  
  if (strstr(key, "\"shuffle\"")) {
      char *p = strstr(key, "\"shuffle\"");
      p = strchr(p, ':');
      p++;
      s->shuffle = atoi(p);
  }
  
  if (strstr(key, "\"loop\"")) {
      char *p = strstr(key, "\"loop\"");
      p = strchr(p, ':');
      p++;
      s->looping = atoi(p);
  }
  
  if (strstr(key, "\"playback_running\"")) {
      char *p = strstr(key, "\"playback_running\"");
      p = strchr(p, ':');
      p++;
      s->running = atoi(p);
  }
  
  // Parse metadata section (always parse regardless of status section)
  char *p;
  
  // TITLE
  if ((p = strstr(key, "\"title\""))) {
      p = strchr(p, ':');
      p++;
      p++;  // skip opening quote
      int i = 0;
      while (*p && *p != '"' && i < 255) {
          s->metadata.title[i++] = *p++;
      }
      s->metadata.title[i] = '\0';
  }
  
  // ARTIST
  if ((p = strstr(key, "\"artist\""))) {
      p = strchr(p, ':');
      p++;
      p++;
      int i = 0;
      while (*p && *p != '"' && i < 255) {
          s->metadata.artist[i++] = *p++;
      }
      s->metadata.artist[i] = '\0';
  }
  
  // ALBUM
  if ((p = strstr(key, "\"album\""))) {
      p = strchr(p, ':');
      p++;
      p++;
      int i = 0;
      while (*p && *p != '"' && i < 255) {
          s->metadata.album[i++] = *p++;
      }
      s->metadata.album[i] = '\0';
  }
  
  // ALBUM ARTIST
  if ((p = strstr(key, "\"album_artist\""))) {
      p = strchr(p, ':');
      p++;
      p++;
      int i = 0;
      while (*p && *p != '"' && i < 255) {
          s->metadata.album_artist[i++] = *p++;
      }
      s->metadata.album_artist[i] = '\0';
  }
  
  // GENRE
  if ((p = strstr(key, "\"genre\""))) {
      p = strchr(p, ':');
      p++;
      p++;
      int i = 0;
      while (*p && *p != '"' && i < 255) {
          s->metadata.genre[i++] = *p++;
      }
      s->metadata.genre[i] = '\0';
  }
  
  // DATE
  if ((p = strstr(key, "\"date\""))) {
      p = strchr(p, ':');
      p++;
      p++;
      int i = 0;
      while (*p && *p != '"' && i < 255) {
          s->metadata.date[i++] = *p++;
      }
      s->metadata.date[i] = '\0';
  }
  
  // TRACK
  if ((p = strstr(key, "\"track\""))) {
      p = strchr(p, ':');
      p++;
      p++;
      int i = 0;
      while (*p && *p != '"' && i < 255) {
          s->metadata.track[i++] = *p++;
      }
      s->metadata.track[i] = '\0';
  }
  
  // printf("dur = %d pos= %d\n", s->duration, s->position);
  // printf("title:%s\nartist:%s\nalbum:%s\n", 
  //        s->metadata.title, s->metadata.artist, s->metadata.album);
}

int update_status(int fd, int *was_playing, TomuStatus *status, PlaybackQueue *queue)
{
  if (*was_playing && !status->running) {
      // Playback just finished -> send next file if in queue mode
      if (queue->has_queue && queue->dir.count > 0) {
          // Move to next file (or loop/shuffle)
          queue->current_index++;

          if (status->shuffle) {
              queue->current_index = get_rand() % queue->dir.count;
          } else if (queue->current_index >= queue->dir.count) {
              if (status->looping)
                  queue->current_index = 0;
              else {
                ctx.running = 1;
                return 0;
              }
          }

          // char fullpath[2048];
          // snprintf(fullpath, sizeof(fullpath), "%s/%s",
          //          queue->dir.base_path,
          //          queue->dir.files[queue->current_index]);
          // send_path(fd, fullpath);
          // printf("\nPlaying: %s\n", queue->dir.files[queue->current_index]);
      }
  }
  *was_playing = status->running;
  progress(status, status->position, status->duration);
  return 1;
}

void progress(TomuStatus *status, double current_time, int duration_time)
{
  if (duration_time == 0) return;


  int pos = (int)((current_time / duration_time) * (*bar_width));
    printf("\033[2K\r");              // clear current line, carriage return
  printf("\r%s%s", status->paused ? " (Paused) " : "", color);
  for (int i = 0; i < *bar_width; i++) {
    if      (i < pos)  printf("%s", done);
    else if (i == pos) printf("%s", current);
    else               printf("%s", remaining);
  }
  printf(" %s %d:%02d:%02d / %d:%02d:%02d (~%.0f%%) | %.2fx v:%.0f%% s:%d l:%d",
    WHT,
    get_hour(current_time), get_min(current_time), get_sec(current_time),
    get_hour(duration_time), get_min(duration_time), get_sec(duration_time),
    (current_time / duration_time) * 100.0,
    status->speed, status->volume * 100.0f,
    status->shuffle, status->looping
  );
  fflush(stdout);
}
