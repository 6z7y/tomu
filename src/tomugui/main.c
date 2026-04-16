#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <termios.h>
#include <signal.h>

#include "utils.h"
#include "../shared/share_data.h"

// init client ctx
Client_CTX client_ctx = {0};

int main(int argc, char **argv)
{
  signal(SIGINT, sig_clean); // when prog close by ctrl+c

  // 1. connect to server
  int *server_fd = &client_ctx.server_fd; // socket fd session between server
  socket_mode(server_fd, 1);

  // 2. send client type
  ClientType my_type = CLIENT_TUI;
  write(*server_fd, &my_type, sizeof(ClientType));

  // 3. init poll
  struct pollfd fds[2]; // handle 2 fd sync it
  fds[0] = (struct pollfd){ .fd= *server_fd, .events= POLLIN };    // socket handle
  // fds[1] = (struct pollfd) { .fd= STDIN_FILENO, .events= POLLIN }; // stdin termios handle

  char key[8];

  // 7. main loop
  // while(1) {
  //   int ret = poll(fds, 2, 800); // (800ms) for ret == 0 
  //   if (ret < 0) { continue; }
  //
  //   if (ret == 0) {}  // timeout = silence = song ended
  //
  //   if (fds[0].revents & POLLIN) {}
  //
  //   if (fds[1].revents & POLLIN) {}
  // }
  printf("not yet!");


  socket_mode(&*server_fd, 0);

bye:
  return 0;
}

