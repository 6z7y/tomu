#include <stdio.h>
#include <unistd.h>

#include "backend.h"
#include "../shared/share_utils.h"
#include "CLIENT_DATA.h"


void print_header(const char *filename)
{
  printf("\033[?25l");
  printf("Playing: %s\n", filename);
  // printf("%.2dHz, %dch, %s\n", sample_rate, channels, fmt_name);  // row 2
  fflush(stdout);
}

void progress(TomuStatus *status, double current_time, int duration_time)
{
  if (duration_time == 0) return;

  const char done = ctx.cfg.progress.done;
  const char current = ctx.cfg.progress.current;
  const char remaining = ctx.cfg.progress.remaining;

  int bar_width = 32;
  int pos = (int)((current_time / duration_time) * bar_width);
    printf("\033[2K\r");              // clear current line, carriage return
  printf("\r%s[", status->paused ? " (Paused) " : "");
  for (int i = 0; i < bar_width; i++) {
    if      (i < pos)  printf("%c", done);
    else if (i == pos) printf("%c", current);
    else               printf("%c", remaining);
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
