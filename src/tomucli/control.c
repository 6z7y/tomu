#include <string.h>
#include <unistd.h>
#include <termios.h>

#include "control.h"
#include "utils.h"
#include "../shared/share_backend.h"

inline void send_cmd(int *server_fd, Command cmd)
{ write(*server_fd, &cmd, sizeof(Command)); }

typedef struct {
  char *key; // stdin (keyborard keys)
  Command cmd; // Command enum send to server and will parse it
} KeyMap;

static const KeyMap keymap[] = {
//    KEY             EXEC
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

void handle_control(int *server_fd, const char *key)
{
  for (int i = 0; i < sizeof(keymap)/sizeof(keymap[0]); i++) {
    if (!strcmp(key, keymap[i].key)) {
      send_cmd(server_fd, keymap[i].cmd);
      // printf("%s\n", key );

      }

  }

  if (!strcmp(key, "q")) {
    printf("hi\n");
    clean_with_bye(server_fd);
  }
}
