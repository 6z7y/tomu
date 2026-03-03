#include <stdio.h>
#include <unistd.h>

#include "backend.h"
#include "../share_clients.h"

void progress(TomuStatus *status, double current_time, int duration_time)
{
  if (duration_time == 0) return;

  int bar_width = 30;
  int pos = (int)((current_time / duration_time) * bar_width);

  printf("\0337");
  printf("\033[0J");
  printf("\r%s[", status->paused ? " (Paused) " : "");
  for (int i = 0; i < bar_width; i++) {
    if      (i < pos)  printf("=");
    else if (i == pos) printf(">");
    else               printf(".");
  }
  printf("] %d:%02d:%02d / %d:%02d:%02d (~%.0f%%) | %.2fx v:%.0f%% s:%d l:%d\r",
    get_hour(current_time), get_min(current_time), get_sec(current_time),
    get_hour(duration_time), get_min(duration_time), get_sec(duration_time),
    (current_time / duration_time) * 100.0,
    status->speed, status->volume * 100.0f,
    status->shuffle, status->loop
  );
  printf("\0338");
  fflush(stdout);
}
