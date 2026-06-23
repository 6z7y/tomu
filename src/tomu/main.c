#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <dbus/dbus.h>
#include <stdio.h>

#include "DATA.h"
#include "args.h"
#include "config.h"
#include "mpris.h"
#include "utils.h"

PlayBackContext ctx = {0};

int main(int argc, char **argv)
{
    signal(SIGINT, sig_clean);
    signal(SIGPIPE, SIG_IGN);

    if (argc > 1) {
      if (args_handle(argv[argc - 1]) == 1) goto bye;
    }

    load_config();
    
    mpris_init();

    while (1) {
        mpris_dispatch();
        if (ctx.state.running) {
            mpris_notify_change();
        }
        sleep_ms(100);
    }

bye:
    return 0;
}
