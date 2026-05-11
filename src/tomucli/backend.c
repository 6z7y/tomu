#include <stdio.h>
#include <unistd.h>

#include "backend.h"
#include "control.h"
#include "../shared/share_utils.h"
#include "CLIENT_DATA.h"

  static const int *bar_width = &ctx.cfg.progress.width;
  static const char *done = &ctx.cfg.progress.done;
  static const char *current = &ctx.cfg.progress.current;
  static const char *remaining = &ctx.cfg.progress.remaining;

int update_status(int fd, int *was_playing, TomuStatus *status, PlaybackQueue *queue)
{
  if (*was_playing && !status->playback_running) {
      // Playback just finished -> send next file if in queue mode
      if (queue->has_queue && queue->dir.totalFiles > 0) {
          // Move to next file (or loop/shuffle)
          queue->current_index++;
          if (status->shuffle) {
              queue->dir.rand_num = get_rand();
              queue->current_index = queue->dir.rand_num % queue->dir.totalFiles;
          } else if (queue->current_index >= queue->dir.totalFiles) {
              if (status->loop)
                  queue->current_index = 0;
              else {
                ctx.running = 1;
                return 0;
              }
          }

          char fullpath[2048];
          snprintf(fullpath, sizeof(fullpath), "%s/%s",
                   queue->dir.base_path,
                   queue->dir.files[queue->current_index]);
          send_path(fd, fullpath);
          printf("\nPlaying: %s\n", queue->dir.files[queue->current_index]);
      }
  }
  *was_playing = status->playback_running;
  progress(status, status->position, status->duration);
  return 1;
}

// void print_header(const char *filename)
// {
//   printf("\033[?25l");
//   printf("Playing: %s\n", filename);
//   // printf("%.2dHz, %dch, %s\n", sample_rate, channels, fmt_name);  // row 2
//   fflush(stdout);
// }

void progress(TomuStatus *status, double current_time, int duration_time)
{
  if (duration_time == 0) return;


  int pos = (int)((current_time / duration_time) * (*bar_width));
    printf("\033[2K\r");              // clear current line, carriage return
  printf("\r%s[", status->paused ? " (Paused) " : "");
  for (int i = 0; i < *bar_width; i++) {
    if      (i < pos)  printf("%c", *done);
    else if (i == pos) printf("%c", *current);
    else               printf("%c", *remaining);
  }
  printf("] %d:%02d:%02d / %d:%02d:%02d (~%.0f%%) | %.2fx v:%.0f%% s:%d l:%d",
    get_hour(current_time), get_min(current_time), get_sec(current_time),
    get_hour(duration_time), get_min(duration_time), get_sec(duration_time),
    (current_time / duration_time) * 100.0,
    status->speed, status->volume * 100.0f,
    status->shuffle, status->loop
  );
  fflush(stdout);
}
