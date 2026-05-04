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
#include "CLIENT_DATA.h"
#include "file_handle.h"

// arg compare opts
static const char *help_opts[] = {"--help", "help", "-h", "h", NULL};    // help
static const char *ver_opts[]  = {"--version", "version", "-v", "v", NULL}; // version

typedef struct {
  const char *option;
  Command cmd;
  const char *desc;
} list_cmd;

static const list_cmd cmd_table[] = {
/*     option           cmd                       desc               */
    {"set",         CMD_PATH,              "Send File path to queue"},
    {"toggle",      CMD_PLAY_TOGGLE,       "Play / pause toggle"},
    {"stop",        CMD_STOP,              "Stop playback"},
    {"next",        CMD_NEXT_AUDIO,        "Next audio track"},
    {"prev",        CMD_PREV_AUDIO,        "Previous audio track"},
    {"volup",       CMD_VOL_UP,            "Increase volume"},
    {"voldown",     CMD_VOL_DOWN,          "Decrease volume"},
    {"slow",        CMD_SPEED_SLOW,        "Slow playback speed"},
    {"fast",        CMD_SPEED_FAST,        "Fast playback speed"},
    {"normal",      CMD_SPEED_DEFAULT,     "Reset playback speed"},
    {"seek+5",      CMD_SEEK_FORWARD_5S,   "Seek forward 5 seconds"},
    {"seek+60",     CMD_SEEK_FORWARD_1M,   "Seek forward 1 minute"},
    {"seek-5",      CMD_SEEK_BACKWARD_5S,  "Seek backward 5 seconds"},
    {"seek-60",     CMD_SEEK_BACKWARD_1M,  "Seek backward 1 minute"},
    {"loop",        CMD_LOOP_TOGGLE,       "Toggle loop mode"},
    {"shuffle",     CMD_SHUFFLE_TOGGLE,    "Toggle shuffle mode"},
    {NULL, 0, NULL}
};


int match_opt(const char *arg, const char **opts)
{
  for (int i=0; opts[i]; i++)
      if (!strcmp(arg, opts[i])) return 1;

  return 0;
}

int handle_remote(int fd, const char *option, const char *path)
{
  for (int i=0; cmd_table[i].option != NULL; i++) {

    if (!strcmp(cmd_table[i].option, option)) {
        if (i == 0) send_path(ctx.server_fd, path);

        else {
          write_now_enum(fd, Command, cmd_table[i].cmd);
        }
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
    "   --version, -v        : show version of program\n\n"

    " Remote commands:\n"
    );

  for (int i=0; cmd_table[i].option != NULL; i++) {
    printf("    %s:      (%s)\n", cmd_table[i].option, cmd_table[i].desc);
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

int args_handle(int fd, int argc, char **argv)
{
  char *option = argv[1];
  char *path = argv[argc - 1]; // for arg ( set [FILE] )

    if      (is_valid_path(option)) return 0; // check path exitsts

    else if (!handle_remote(fd, option, path)) goto last; // remote command

    else if (match_opt(option, help_opts)) help();

    else if (match_opt(option, ver_opts)) printf("tomucli: %s\n", TOMUCLI_VER);

    else    die("[T]: unknown option/Path '%s'\n\ntry '%s --help'", option, TOMUCLI_NAME);

last:
  return 1;
}
