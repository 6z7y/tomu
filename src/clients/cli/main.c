#include <stdio.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <termios.h>
#include <poll.h>
#include "control.h"
#include "utils.h"
#include "../../share.h"
#include "../share-clients.h"

int main(int argc, char **argv)
{
  // See what the user wants with "--" and handle it
  if ( argv[1][0] == '-' && argv[1][1] == '-' ){

    if ( strcmp(argv[1], "--help") == 0 )
      help();

    else if ( strcmp(argv[1], "--version") == 0 )
      printf("Tomu: %s\n", CLIENT_CLI_VER);

    else 
      printf("[T]: unknown option '%s'\n", argv[1]);

    return 0;
  }

  // 1. create socket
  struct sockaddr_un addr = { .sun_family = AF_UNIX, .sun_path = SOCKET_PATH };
  int serverfd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (serverfd < 0) die("Socket:");

  // 2. Connect socket
  if (connect(serverfd, (struct sockaddr*)&addr, sizeof(addr)) < 0)
    die("Connect failed, make sure server running");

  // 3. if path given, send it to server
  if (argc > 1)
    send_path(serverfd, argv[argc - 1]);

  // 4. enter termios mode
  termios_mode(1);

  // 5. init poll
  struct pollfd fds[2];
  fds[0].fd = serverfd;
  fds[0].events = POLLIN;
  fds[1].fd = STDIN_FILENO;
  fds[1].events = POLLIN;

  char key[8];
  char buf[256];

  // 6. main loop
  while(1) {
    int ret = poll(fds, 2, 1000);
    if (ret <= 0) { continue; }

    if (fds[0].revents & POLLIN) {
      int n = read(serverfd, buf, sizeof(buf) - 1);
      if (n > 0) {
        buf[n] = '\0';
        printf("\r%s", buf);
        fflush(stdout);
      }
      else if (n <= 0) { perror("Server disconnected"); break; }
    }

    if (fds[1].revents & POLLIN) {
      int n = read(STDIN_FILENO, key, sizeof(key) - 1);
      key[n] = '\0';
      handle_control(serverfd, key);
    }
  }

  clean_with_bye(serverfd, 0);
}
