#include <string.h>
#include <unistd.h>
#include <termios.h>

#include "control.h"
#include "utils.h"

#include "CLIENT_DATA.h"
#include "../shared/share_utils.h"

typedef struct {
  char *key; // stdin (keyborard keys)
  Command cmd; // Command enum send to server and will parse it
} KeyMap;

static const KeyMap keymap[] = {
  /*  KEY               EXEC             */
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
    { "s",          CMD_SHUFFLE_TOGGLE   },
};

void handle_control(int *server_fd, const char *key)
{
  for_each_arr(keymap) {
    if (!strcmp(key, keymap[i].key)) {
      write_now_enum(*server_fd, Command, keymap[i].cmd);
    }
  }

  if (!strcmp(key, "q")) ctx.running = 0;
}

// change raw mode terminal 1=on, 0=off
void termios_mode(int mode)
{
  // 1. get terminal settings
  static struct termios old; 

  // 2. modify with used
  if (mode == 1){
    struct termios raw;
    tcgetattr(STDIN_FILENO, &raw);
    old = raw;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    printf("\033[?25l"); // hide cursor
  }

  else tcsetattr(STDIN_FILENO, TCSANOW, &old);
}
