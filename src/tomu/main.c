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
  signal(SIGINT, signal_handle);
  signal(SIGTERM, signal_handle);
  // signal(SIGPIPE, SIG_IGN);

  char *option = argv[1];
  char *src = argv[argc-1];

  if (argc > 1) args_handle(option, src);

  first_init();

  // Main loop - handles D-Bus events and keeps the program running
  mpris_loop();
}
