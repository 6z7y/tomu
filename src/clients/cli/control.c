#include <string.h>
#include <unistd.h>
#include <termios.h>

#include "control.h"
#include "utils.h"
#include "../share_clients.h"

static const KeyMap keymap[] = {
//    KEY             EXEC
    { "q",          CMD_STOP             },
    { " ",          CMD_PLAY_TOGGLE      },
    { "\n",         CMD_NEXT_AUDIO       },
    { ">",          CMD_NEXT_AUDIO       },
    { "<",          CMD_PREV_AUDIO       },
    { "+",          CMD_VOL_UP           },
    { "-",          CMD_VOL_DOWN         },
    { "]",          CMD_SPEED_FAST       },
    { "[",          CMD_SPEED_SLOW       },
    { "\177",       CMD_SPEED_DEFAULT    },
    { "\x1b[C",     CMD_SEEK_FORWARD_5S  },
    { "\x1b[D",     CMD_SEEK_BACKWARD_5S },
    { "\x1b[A",     CMD_SEEK_FORWARD_1M  },
    { "\x1b[B",     CMD_SEEK_BACKWARD_1M },
    { "l",          CMD_LOOP_TOGGLE      },
    { "s",          CMD_SHUFFLE_TOGGEL   },
};

void handle_control(int sock, const char *key)
{
  for (int i = 0; i < sizeof(keymap)/sizeof(keymap[0]); i++) {
    if (!strcmp(key, keymap[i].key)) {
      send_cmd(sock, keymap[i].cmd);
      // printf("%s\n", key );

      if (!strcmp(key, "q")) clean_with_bye(sock, 0);
      if (!strcmp(key, "h")) help();

      return;
    }
  }
}

void send_cmd(int sock, Command cmd)
{
  write(sock, &cmd, sizeof(Command));
}


// Add this to client/control.c

// Send a file/dir path to server for playback
void send_path(int sock, const char *path)
{
  Command cmd = CMD_PATH;
  int pathlen = strlen(path);

  // send: CMD_PATH | length | path
  write(sock, &cmd,     sizeof(Command));
  write(sock, &pathlen, sizeof(int));
  write(sock, path,     pathlen);
}

// In handle_control(), add key "p" → ask for path:

// void path_change(int sock, const char *key)
// {
//   if (!strcmp(key, "p")) {
//     termios_mode(0);          // back to normal mode so user can type
//     char path[1024];
//     printf("Path: ");
//     fgets(path, sizeof(path), stdin);
//     path[strcspn(path, "\n")] = 0;  // remove newline
//     send_path(sock, path);
//     termios_mode(1);          // back to raw mode
//     return;
//
//   }
// }
