#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <dbus/dbus.h>
#include <stdio.h>
#include <curl/curl.h>

#include "macros.h"
#include "structs.h"
#include "args.h"
#include "mpris.h"
#include "utils.h"

PlayBackContext tctx = {0};

int main(int argc, char **argv)
{
  signal(SIGINT, sig_clean);
  signal(SIGPIPE, SIG_IGN);

  if (argc > 1) {
      if (args_handle(argv[argc - 1])) {
          if (!tctx.playback_active) exit(0);
      }
  }

  first_init();

  // Main loop - handles D-Bus events and keeps the program running
  mpris_loop();
}
