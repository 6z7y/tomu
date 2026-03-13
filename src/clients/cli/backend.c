#include <stdio.h>
#include <unistd.h>

#include "backend.h"
#include "../share_backend.h"

void print_header(const char *filename, int sample_rate, int channels, const char *fmt_name)
{
  printf("\033[H");     // move to top of screen (row 1, col 1)
  printf("\033[2J");    // clear entire screen
  printf("\033[1;1H");  // row 1: filename
  printf("Playing: %s\n", filename);
  printf("%.2dHz, %dch, %s\n", sample_rate, channels, fmt_name);  // row 2
  fflush(stdout);
}

void progress(TomuStatus *status, double current_time, int duration_time)
{
  if (duration_time == 0) return;

  int bar_width = 30;
  int pos = (int)((current_time / duration_time) * bar_width);

  // Move to line 3 (below the 2 header lines) and clear from there down
  printf("\033[3;1H");   // move cursor to row 3, col 1
  printf("\033[0J");     // clear from cursor to end of screen

  printf("%s[", status->paused ? " (Paused) " : "");
  for (int i = 0; i < bar_width; i++) {
    if      (i < pos)  printf("=");
    else if (i == pos) printf(">");
    else               printf(".");
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
