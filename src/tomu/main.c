#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <dbus/dbus.h>
#include <stdio.h>
#include <curl/curl.h>

#include "structs.h"
#include "args.h"
#include "mpris.h"
#include "utils.h"

PlayBackContext tctx = {0};

#define IS_URL(arg) (!strncmp(arg, "http://", 7)) || (!strncmp(arg, "https://", 8))

int main(int argc, char **argv)
{
  signal(SIGINT, sig_clean);
  signal(SIGPIPE, SIG_IGN);

  printf("is url: %d\n", IS_URL(argv[argc-1]));
  return 0;

  first_init();

  if (argc > 1) {
      if (args_handle(argv[argc - 1])) {
          if (!tctx.playback_active) {
              goto bye;
          }
      }
  }

  // Main loop - handles D-Bus events and keeps the program running
  mpris_loop();

bye:
  curl_global_cleanup();
  return 0;
}
