#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <termios.h>

#include "control.h"

#include "CLIENT_DATA.h"
#include "../shared/share_utils1.h"
#include "args.h"

typedef struct {
  int key; // stdin (keyborard keys)
  Command cmd; // Command enum send to server and will parse it
} KeyMap;

static const KeyMap keymaps[] = {
  /*  KEY               EXEC             */
    { ' ',          CMD_PLAY_TOGGLE      },
    { KY_ENTER,     CMD_NEXT_AUDIO       },
    { '>',          CMD_NEXT_AUDIO       },
    { '<',          CMD_PREV_AUDIO       },
    { '+',          CMD_VOL_UP           },
    { '-',          CMD_VOL_DOWN         },
    { ']',          CMD_SPEED_FAST       },
    { '[',          CMD_SPEED_SLOW       },
    { KY_BACKSPACE, CMD_SPEED_DEFAULT    },
    { KY_RIGHT,     CMD_SEEK_FORWARD_5S  },
    { KY_LEFT,      CMD_SEEK_BACKWARD_5S },
    { KY_UP,        CMD_SEEK_FORWARD_1M  },
    { KY_DOWN,      CMD_SEEK_BACKWARD_1M },
    { 'l',          CMD_LOOP_TOGGLE      },
    { 's',          CMD_SHUFFLE_TOGGLE   },
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

int read_key()
{
  char ch;
  if (read(STDIN_FILENO, &ch, 1) <= 0) {
    warn("something happend:");
    return -1;

  }

  // for checking about arrow keys it's like '27[A', 27=ESC, [, A=UP/B=DN/C=RGT/D=LFT
  if (ch == KY_ESC) { // it's ESC
    char seq[2];
    if (read(STDIN_FILENO, &seq[0], 1) <= 0) return KY_ESC; // must get '['
    if (read(STDIN_FILENO, &seq[1], 1) <= 0) return KY_ESC; // must get 'A'/'B'/'C'/'D'
    if (seq[0] == '[') {
      if (seq[1] == 'A') return KY_UP;
      if (seq[1] == 'B') return KY_DOWN;
      if (seq[1] == 'C') return KY_RIGHT;
      if (seq[1] == 'D') return KY_LEFT;
    }

    return KY_ESC; // else not arrow keys return ESC 
  }
  else if (ch == '\n') return KY_ENTER;

  return ch;
}

void handle_control(int server_fd)
{
  int ch = read_key();
  // printf("\n\n%c=%d\n\n", ch, ch);

  if (ch == 'q') ctx.running = 0;
  else if (ch == '?') help_keys();

  for_each_arr(keymaps) {
    if (ch == keymaps[i].key) {
      write_now_enum(server_fd, Command, keymaps[i].cmd);
    }
  }
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
