#include "utils.h"
#include <stdio.h>
#include <unistd.h>
#include <termios.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "../share_clients.h"
#include "../../share_info.h"

void clean_with_bye(int socket, int mode)
{
  termios_mode(0);
}

void socket_mode(int mode, int *server_fd)
{
  if (mode) {
    // 1. create socket
    struct sockaddr_un addr = { .sun_family = AF_UNIX, .sun_path = SOCKET_PATH };
    *server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (*server_fd < 0) die("Socket:");

    // 2. Connect socket
    if (connect(*server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
      die("Connect failed, make sure server running");
  }

  else {
    close(*server_fd);
  }
}


// change mode terminal 1=on, 0=off
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
  }

  else tcsetattr(STDIN_FILENO, TCSANOW, &old);
}

void help()
{
  printf(
    "Usage: tomu [COMMAND] [PATH]\n"
    " Commands:\n\n"

    "   --loop            : loop same sound\n"
    "   --version         : show version of program\n"
    "   --help            : show help message\n"

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

    "\nExample: tomu loop [FILE.mp3]\n"
  );
}
