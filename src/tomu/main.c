#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <dbus/dbus.h>
#include <stdio.h>
#include <curl/curl.h>

#include "DATA.h"
#include "args.h"
#include "config.h"
#include "mpris.h"
#include "streaming.h"
#include "utils1.h"
#include "utils2.h"

PlayBackContext tctx = {0};

int main(int argc, char **argv)
{
  signal(SIGINT, sig_clean);
  signal(SIGPIPE, SIG_IGN);

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
