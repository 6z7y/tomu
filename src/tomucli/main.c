#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <termios.h>
#include <signal.h>

#include "../shared/shared_control.h"
#include "../shared/share_utils1.h"
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
  
  int was_playing = 0;  // track previous playback_running state

  // 2. read config
  load_config();

  // 3. Connect to socket
  client_socket_mode(server_fd, 1);

  // 4. Load Action
  if (argc > 1) {
    if (args_handle(*server_fd, argc, argv)) goto bye;
    path_handle(argv[argc-1], queue);
  }
  // for_each_num(queue->dir.count) {
  //   printf("%d: '%s'\n", i, queue->dir.files[i]);
  //   sleep_ms(5);
  // }

  // printf("u have files %d\n", queue->dir.count);

  int n;
  Command cmd;
  char json_msg[1024];
  int ch;
  // int ret;
  termios_mode(1);

  // 5. init poll
  struct pollfd fds[2];
  fds[0] = (struct pollfd){ .fd = STDIN_FILENO, .events = POLLIN };
  fds[1] = (struct pollfd){ .fd = *server_fd,   .events = POLLIN };

  // main loop (client will stay here)
  while (ctx.running) {
    int ret = poll(fds, 2, 80);
    if (ret < 0) continue;

    // Keyboard input
    if (fds[0].revents & POLLIN) {
      handle_control(*server_fd);
    }

    // Server status update
    if (fds[1].revents & POLLIN) {
        int n = read(*server_fd, &cmd, sizeof(cmd));
        if (n <= 0) die("server: dissconnected");

        if (cmd == CMD_STATUS) {
          int n = read(*server_fd, &json_msg, sizeof(json_msg));
          if (n <= 0) die("server: dissconnected");

          TomuStatus *s = &ctx.status;
          if (n >= (int)sizeof(json_msg)) n = (int)sizeof(json_msg) - 1;
          json_msg[n] = '\0';
          parse_json(json_msg);
          if (s) {
            if (!update_status(*server_fd, &was_playing, status, queue)) {
              ctx.running = 0;
              continue;
            }
          }
        }
        else if (cmd == CMD_FIRST_INFOS) {
          uint8_t name_len;
          read(ctx.server_fd, &name_len, sizeof(name_len));

          char msg[256];
          int n2 = read(ctx.server_fd, &msg, name_len);
          msg[name_len] = '\0';
          printf("\nPlaying: '%s'\n", msg);

          Audio_Metadata metadat;
          int n_metadata = read(ctx.server_fd, &metadat, sizeof(metadat));

          // printf("looook; %s | %s\n", metadat.album, metadat.artist);

          if (n_metadata == sizeof(metadat)) {
            printf("metadata:\n");
            printf("  %s\n", metadat.artist);
            printf("  %s\n", metadat.album);
            printf("  %s\n", metadat.album_artist);
            printf("  %s\n", metadat.date);
            printf("  %s\n", metadat.genre);
            printf("  %s\n", metadat.track);
          }
        }
        else if (cmd == CMD_END) printf("\n");

        else die("server: Disconnected");
    }

    // Send next file only when idle AND not already waiting
    if (!status->running && queue->dir.count > 0 ) {

        // printf("\n");
        int idx = get_rand() % queue->dir.count;
        send_path(*server_fd, format("%s", queue->dir.files[idx]));
        // printf("Playing: %s\n", queue->dir.files[idx]);

        free(queue->dir.files[idx]);
        queue->dir.files[idx] = queue->dir.files[queue->dir.count - 1];
        queue->dir.count--;
    }
    // else  printf("\n");
  }

bye:
  queue_free(queue);
  termios_mode(0);
  client_socket_mode(server_fd, 0);
  return 0;
}
