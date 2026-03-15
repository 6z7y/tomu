#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <termios.h>

#include "backend.h"
#include "control.h"
#include "utils.h"
#include "../share_backend.h"
#include "../share_data.h"
#include "../../shared/shared_control.h"

int main(int argc, char **argv)
{
  // signal(SIGINT, sig_clean); // when prog close by ctrl+c
  // 1. check args
  if (args_handle(argv[argc - 1]) == 1) goto bye;

  int server_fd; // socket fd session between server
  PlaybackQueue queue = {0}; // about path

  // 2. enter raw terminal
  termios_mode(1);

  // 3. connect to server
  socket_mode(&server_fd, 1);

  // 4. send client type
  ClientType my_type = CLIENT_CLI;
  printf("sizeof clienttype: %zu, and type var: %zu\n", sizeof(ClientType), sizeof(my_type));
  write(server_fd, &my_type, sizeof(ClientType));

  if (argc == 2) path_handle(server_fd, argv[argc-1], &queue);

  TomuStatus status = {0};
  int playback_finished = 0;

  // 4. init poll
  struct pollfd fds[2];
  fds[0].fd = server_fd;
  fds[0].events = POLLIN;
  fds[1].fd = STDIN_FILENO;
  fds[1].events = POLLIN;

  char key[8];

  // 5. main loop
  while(1) {
      int ret = poll(fds, 2, 1000);
      if (ret < 0) { continue; }

      if (ret == 0) {  // timeout = silence = song ended
          if (queue.has_queue && !playback_finished) {
              playback_finished = 1;
              handle_playback_complete(server_fd, &queue);
          }
          continue;
      }

      if (fds[0].revents & POLLIN) {
          TomuStatus tmp;
          int n;
          while ((n = read(server_fd, &tmp, sizeof(tmp))) == (int)sizeof(tmp)) {
              status = tmp;
              playback_finished = 0;

              struct pollfd pfd = { .fd = server_fd, .events = POLLIN };
              if (poll(&pfd, 1, 0) <= 0) break;
          }
          if (n == 0 || (n < 0 && errno != EAGAIN)) {
              fprintf(stderr, "Server disconnected\n");
              break;
          }

          progress(&status, status.position, status.duration);
      }

      if (fds[1].revents & POLLIN) {
          int n = read(STDIN_FILENO, key, sizeof(key) - 1);
          if (n > 0) {
              key[n] = '\0';
              handle_control(&server_fd, key);

              if (!strcmp(key, "\n") || !strcmp(key, ">")) {
                  if (queue.has_queue)
                      handle_playback_complete(server_fd, &queue);
              }
              else if (!strcmp(key, "<")) {
                  if (queue.has_queue) {
                      queue.dir.currentFile--;
                      if (queue.dir.currentFile < 0)
                          queue.dir.currentFile = queue.dir.totalFiles - 1;
                      send_next_from_queue(server_fd, &queue);
                  }
              }
          }
      }
  }

  termios_mode(0);
  socket_mode(&server_fd, 0);

  if (queue.has_queue) {
      for (int i = 0; i < queue.dir.totalFiles; i++)
          free(queue.dir.files[i]);
      free(queue.dir.files);
  }

bye:
  return 0;
}
