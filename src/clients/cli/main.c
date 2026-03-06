#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <termios.h>
#include <poll.h>
#include <errno.h>

#include "backend.h"
#include "control.h"
#include "utils.h"
#include "../share_clients.h"

int main(int argc, char **argv)
{
  int server_fd;

  // 1. connect to server
  socket_mode(1, &server_fd);

  // 2. enter raw terminal
  termios_mode(1);

  // in client main.c, after socket_mode(1, &server_fd):
  ClientType my_type = CLIENT_CLI;  // or TUI/GUI depending on which client
  write(server_fd, &my_type, sizeof(ClientType));

  // 3. if path given, send it to server
  if (argc > 1)
    send_path(server_fd, argv[argc - 1]);

  // Audio_State state = {0};
  TomuStatus status = {0};

  // 4. init poll
  struct pollfd fds[2];
  fds[0] = (struct pollfd){ server_fd, POLLIN };
  fds[1] = (struct pollfd){ STDIN_FILENO, POLLIN };

  // for store thing
  char key[8];
  // char buf[256];

  // 6. main loop
  while(1) {
    int ret = poll(fds, 2, -1);
    if (ret <= 0) { continue; }

    if (fds[0].revents & POLLIN) {
        TomuStatus tmp;
        int n;
        // drain all pending statuses, keep the last one
        while ((n = read(server_fd, &tmp, sizeof(tmp))) == (int)sizeof(tmp)) {
            status = tmp;
            // check if more data waiting
            struct pollfd pfd = { server_fd, POLLIN, 0 };
            if (poll(&pfd, 1, 0) <= 0) break;  // no more data, stop
        }
        if (n == 0 || (n < 0 && errno != EAGAIN)) {
            fprintf(stderr, "Server disconnected\n");
            break;
        }

        progress(&status, status.position, status.duration);
    }

    if (fds[1].revents & POLLIN) {
      int n = read(STDIN_FILENO, key, sizeof(key) - 1);
      key[n] = '\0';
      handle_control(server_fd, key);
    }
  }

  termios_mode(0);
  socket_mode(0, &server_fd);
}
