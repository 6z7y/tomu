#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <termios.h>
#include <signal.h>

#include "../shared/shared_control.h"
#include "../shared/share_utils.h"
#include "CLIENT_DATA.h"
#include "args.h"
#include "backend.h"
#include "config.h"
#include "control.h"
#include "file_handle.h"
#include "utils.h"

Client_CTX ctx = {0};


int main(int argc, char **argv)
{
  signal(SIGINT, sig_clean);

  // 1. init context
  init_context();
  int *server_fd = &ctx.server_fd;
  PlaybackQueue *queue = &ctx.queue;
  TomuStatus *status = &ctx.status;
  
  int first_file_sent = 0;
  int was_playing = 0;  // track previous playback_running state
  int waiting_for_playback = 0;  // add this

  // 2. read config
  load_config();

  // 3. Connect to socket
  client_socket_mode(server_fd, 1);

  // 4. Load Action
  if (argc > 1) {
    if (args_handle(*server_fd, argc, argv)) goto bye;
    path_handle(*server_fd, argv[argc-1], queue);
  }
  int n;
  char key[8];
  termios_mode(1);

  // 5. init poll
  struct pollfd fds[2];
  fds[0] = (struct pollfd){ .fd = STDIN_FILENO, .events = POLLIN };
  fds[1] = (struct pollfd){ .fd = *server_fd,   .events = POLLIN };


  while (ctx.running) {
    int ret = poll(fds, 2, 80);
    if (ret < 0) continue;

    // Keyboard input
    if (fds[0].revents & POLLIN) {
      n = read(STDIN_FILENO, key, sizeof(key) - 1);
      key[n] = '\0';
      handle_control(server_fd, key, n);
    }

    // Server status update
    if (fds[1].revents & POLLIN) {
        int n = read(*server_fd, status, sizeof(*status));
        if (n == sizeof(TomuStatus)) {
            if (status->playback_running)
                waiting_for_playback = 0;  // server confirmed it started playing
            if (!update_status(*server_fd, &was_playing, status, queue)) {
                ctx.running = 0;
                continue;
            }
        }
        else die("server: Disconnected");
    }

    // Send next file only when idle AND not already waiting
    if (!status->playback_running && queue->has_queue 
        && queue->dir.totalFiles > 0 && !waiting_for_playback) {

        int idx = get_rand() % queue->dir.totalFiles;
        send_path(*server_fd, format("%s/%s", queue->dir.base_path, queue->dir.files[idx]));
        printf("Playing: %s\n", queue->dir.files[idx]);

        free(queue->dir.files[idx]);
        queue->dir.files[idx] = queue->dir.files[queue->dir.totalFiles - 1];
        queue->dir.totalFiles--;

        waiting_for_playback = 1;  // block until server responds with running=1
    }
  }

bye:
  queue_free(queue);
  termios_mode(0);
  client_socket_mode(server_fd, 0);
  return 0;
}
