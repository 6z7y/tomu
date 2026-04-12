#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <termios.h>
#include <stdarg.h>

#include "../share_backend.h"
#include "../../shared/share_info.h"

// arg compare opts
static const char *help_opts[] = {"--help", "-h", NULL};    // help
static const char *ver_opts[]  = {"--version", "-v", NULL}; // version

static inline void help()
{
  printf(
    "Usage: tomucli [Dir/OR/File]\n"
    " Options:\n\n"

    // "   --loop            : loop same sound\n"
    "   --help,    -h        : show help message\n"
    "   --version, -v        : show version of program\n"

    "\nkeys:\n"
    " (Space) = pause/resume\n"
    " (Backspace) = reset playback speed\n"
    " (q) = quit\n"
    " (s) = shuffle toggle\n"
    " (l) = loop toggle\n"
    " (-) = decrease volume\n"
    " (+) = increase volume\n"
    " (↑/→) = audio seek forward +5s/1m\n"
    " (←/↓) = audio seek backward -5s/1m\n"
    " ([) = audio speed decrease\n"
    " (]) = audio speed increase\n"
    " (</>) = (Pervious/Next) audio\n"

    "\nExample: tomu [FILE.mp3]\n"
  );
}

static inline int match_opt(const char *arg, const char **opts)
{
  for (int i=0; opts[i]; i++)
      if (!strcmp(arg, opts[i])) return 1;

  return 0;
}

int args_handle(const char *option)
{
  if (!option) return 0; // no arguments given, continue normally

  // there "--arg"
  if (option[0] == '-') {

    if      (match_opt(option, help_opts)) help();

    else if (match_opt(option, ver_opts)) printf("tomucli: %s\n", TOMUCLI_VER);

    else    warn("[T]: unknown option '%s'\n\ntry '%s --help'", option, TOMUCLI_NAME);

    return 1;
  }

  return 0;
}
