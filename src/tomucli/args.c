#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <termios.h>
#include <stdarg.h>

#include "../shared/SHARE_DATA.h"
#include "../shared/share_utils.h"

// arg compare opts
static const char *help_opts[] = {"--help", "help", "-h", "h", NULL};    // help
static const char *ver_opts[]  = {"--version", "version", "-v", "v", NULL}; // version

typedef struct {
  const char *option;
  Command cmd;
} list_cmd;

static const list_cmd cmd_table[] = {
    {"toggle",  CMD_PLAY_TOGGLE},
    {"stop",    CMD_STOP},
    {"next",    CMD_NEXT_AUDIO},
    {"prev",    CMD_PREV_AUDIO},
    {"volup",   CMD_VOL_UP},
    {"voldown", CMD_VOL_DOWN},
    {"slow",    CMD_SPEED_SLOW},
    {"fast",    CMD_SPEED_FAST},
    {"normal",  CMD_SPEED_DEFAULT},
    {"seek+5",  CMD_SEEK_FORWARD_5S},
    {"seek+60", CMD_SEEK_FORWARD_1M},
    {"seek-5",  CMD_SEEK_BACKWARD_5S},
    {"seek-60", CMD_SEEK_BACKWARD_1M},
    {"loop",    CMD_LOOP_TOGGLE},
    {"shuffle", CMD_SHUFFLE_TOGGLE},
    {NULL, 0}
};

inline int match_opt(const char *arg, const char **opts)
{
  for (int i=0; opts[i]; i++)
      if (!strcmp(arg, opts[i])) return 1;

  return 0;
}

int handle_remote(int fd, const char *option)
{
  for (int i=0; cmd_table[i].option != NULL; i++) {
    if (!strcmp(cmd_table[i].option, option)) {
      write_now_enum(fd, cmd_table[i].cmd);
      return 0;
    }
  }
  return 1;
}

static inline void help()
{
  printf(
    "Usage: tomucli [Dir/OR/File]\n"
    " Options:\n\n"

    "   --help,    -h        : show help message\n"
    "   --version, -v        : show version of program\n"

    " Remote commands:\n"
    );

  for (int i=0; cmd_table[i].option != NULL; i++) {
    printf(" %s\n", cmd_table[i].option);
  }

  printf(
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

int args_handle(int fd, const char *option)
{
  if (is_valid_path(option)) return 0; // check path exitsts

  if (!handle_remote(fd, option)) goto last; // remote command

  else if (match_opt(option, help_opts)) help();

  else if (match_opt(option, ver_opts)) printf("tomucli: %s\n", TOMUCLI_VER);

  else    warn("[T]: unknown option '%s'\n\ntry '%s --help'", option, TOMUCLI_NAME);

last:
  return 1;
}
