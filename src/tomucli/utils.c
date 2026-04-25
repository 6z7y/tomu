#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <termios.h>
#include <stdarg.h>

#include "CLIENT_DATA.h"
#include "control.h"
#include "utils.h"
#include "../shared/SHARE_DATA.h"
#include "../shared/share_utils.h"

void sig_clean(int sig)
{
  client_socket_mode(&client_ctx.server_fd, 0);
  termios_mode(0);
  _exit(0);
}

void clean_with_bye(int *server_fd)
{
  client_socket_mode(server_fd, 0);
  termios_mode(0);
  _exit(0);
}

// mode ( 1 = start ), ( 0 = close )
void client_socket_mode(int *server_fd, int mode)
{
  // mode 1 (start)
  if (mode) {
    // 1. create socket
    struct sockaddr_un addr = { .sun_family = AF_UNIX, .sun_path = SOCKET_PATH };
    *server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (*server_fd < 0) die("Socket:");

    // 2. Connect socket
    if (connect(*server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
      die("Connect failed, make sure server running");
  }

  else close(*server_fd); // mode 0 (close)
}
