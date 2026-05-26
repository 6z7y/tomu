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
#include "file_handle.h"

void sig_clean(int sig)
{
  queue_free(&ctx.queue);
  client_socket_mode(&ctx.server_fd, 0);
  termios_mode(0);
  _exit(0);
}

// mode ( 1 = start ), ( 0 = close )
void client_socket_mode(int *server_fd, int ON)
{
  // mode 1 (start)
  if (ON) {
    // 1. create socket
    struct sockaddr_un addr = { .sun_family = AF_UNIX, .sun_path = SOCKET_PATH };

    if ((*server_fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) die("Socket:");

    // 2. Connect socket
    if (connect(*server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
      die("Connect failed, make sure server running");
    
    // 3. send type client
    write_now_enum(*server_fd, ClientType, CLIENT_CLI);
  }

  else close(*server_fd); // mode 0 (close)
}

void init_context()
{
  ctx.running = 1;
  // ctx.queue.current_index = 0;     // Start at first file
  // ctx.queue.file_played = 0;       // No file played yet
  ctx.queue.dir.rand_num = get_rand();
}
