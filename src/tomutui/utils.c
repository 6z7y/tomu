#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <termios.h>
#include <stdarg.h>

#include "utils.h"
#include "../shared/share_backend.h"
#include "../shared/share_info.h"

void sig_clean(int sig)
{
  socket_mode(&client_ctx.server_fd, 0);
  _exit(0);
}

// mode ( 1 = start ), ( 0 = close )
void socket_mode(int *server_fd, int mode)
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
