#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#include "control.h"

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

// Send a file/dir path to server for playback
void send_path(int server_fd, const char *path)
{
    Command cmd = CMD_PATH;
    int pathlen = strlen(path);

    // send: CMD_PATH | length | path
    if (write(server_fd, &cmd,     sizeof(Command)) != sizeof(Command)) die("[T] can't send command");
    if (write(server_fd, &pathlen, sizeof(int)) != sizeof(int)) die("[T] can't send path len");
    if (write(server_fd, path,     pathlen) != pathlen) die("[T] can't send path data");
}

void handle_control(int *server_fd, const char *key, int n)
{
  for_each_arr(keymap) {
    if (!strcmp(key, keymap[i].key)) {
      write_now_enum(*server_fd, Command, keymap[i].cmd);
    }
  }

  if (!strcmp(key, "q")) ctx.running = 0;
}

struct termios old; 

// change raw mode terminal 1=on, 0=off
void termios_mode(int ON)
{

  // 2. modify with used
  if (ON){
    tcgetattr(STDIN_FILENO, &old); // 
    struct termios raw = old;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    printf("\033[?25l"); // hide cursor
  }

  else tcsetattr(STDIN_FILENO, TCSANOW, &old);
}
