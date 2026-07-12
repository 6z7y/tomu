#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <dbus/dbus.h>
#include <curl/curl.h>

#include "args.h"
#include "errors.h"
#include "player.h"
#include "structs.h"
#include "utils.h"

PlayBackContext tctx = {0};

int main(int argc, char **argv)
{
  if (signal(SIGINT, signal_handle) == SIG_ERR) die("signal SIGINT");
  if (signal(SIGTERM, signal_handle) == SIG_ERR) die("signal SIGTERM");

  if (argc > 1) args_handle(argc, argv);

  first_init();
  playback_handle();
}
