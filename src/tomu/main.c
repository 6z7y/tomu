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
#include "utils.h"

PlayBackContext tctx = {0};

int main(int argc, char **argv)
{
  signal(SIGINT, sig_clean);
  signal(SIGPIPE, SIG_IGN);

  // Initialize curl globally
  curl_global_init(CURL_GLOBAL_ALL);    

  // Initialize MPRIS
  mpris_init();

  // Handle command-line arguments
  if (argc > 1) {
      // args_handle returns 1 if it handled an option that should exit
      // or if there was an error
      if (args_handle(argv[argc - 1])) {
          // If args_handle returned 1, it means we should exit
          // (either help/version was shown, or there was an error)
          // But only if we're not already playing something
          if (!tctx.playback_active) {
              // Clean up and exit
              goto bye;
          }
      }
  }

  // Main loop - handles D-Bus events and keeps the program running
  while (1)
  {
    mpris_dispatch();
    if (tctx.state.running) mpris_notify_change();
    sleep_ms(100);
  }

bye:
  curl_global_cleanup();
  return 0;
}
