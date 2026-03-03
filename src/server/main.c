#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <unistd.h>
#include <pthread.h>

#include "backend.h"
#include "control.h"
#include "utils.h"

static int playback_active = 0;
static pthread_t playback_thread;


void *start_playback_thread(void *arg) {
    char *path = (char*)arg;
    path_handle(path, 0, 1, 0);
    free(path);
    playback_active = 0;  // mark done when playback ends
    return NULL;
}

void start_playback(const char *path) {
    // If already playing, stop it first
    if (playback_active) {
        playback_stop(&ctx.state);
        pthread_join(playback_thread, NULL);
        playback_active = 0;
    }

    char *path_copy = strdup(path);
    pthread_create(&playback_thread, NULL, start_playback_thread, path_copy);
    playback_active = 1;
}


int main(int argc, char **argv)
{
  int server_fd, client_fd;

  // 1. unlink old socket file
  unlink(SOCKET_PATH);

  // 2. initlize socket protocol
  struct sockaddr_un addr = { .sun_family = AF_UNIX, .sun_path = SOCKET_PATH };
  server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd < 0) die("Socket:");

  // 3. bind socket to file
  if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) die("Bind:");

  // 4. inter listen mode
  if (listen(server_fd, MAX_CLIENT) < 0) die("Listen:");

  // 5. client array
  Client_connection client[MAX_CLIENT] = {0};

  // 6. init poll
  struct pollfd fds[1 + MAX_CLIENT];
  fds[0] = (struct pollfd){ .fd = server_fd, .events = POLLIN }; // [0] server

  // 7. main loop
  while(1) {

      // 8. check if client active
      int nfds = 1;
      for (int i=0; i<MAX_CLIENT; i++) {
        if (client[i].active) {
          fds[nfds] = client[i].pfd;
          nfds++;
        }
      }

      // 9. start poll checking
      int ret = poll(fds, nfds, 130);
      if (ret < 0) { warn("Poll ret:"); continue; }

      // 10. ALWAYS broadcast status to all active clients
      TomuStatus status = {
        .duration  = ctx.state.duration,
        .position  = ctx.state.position,
        .paused    = ctx.state.paused,
        .volume    = ctx.state.volume,
        .speed     = ctx.state.speed,
        .shuffle   = ctx.state.shuffle,
        .loop      = ctx.state.looping,
      };
      if (playback_active) {
        for (int i=0; i<MAX_CLIENT; i++) {
          if (client[i].active) {
            if (write(client[i].fd, &status, sizeof(status)) <= 0) {
              // client died while writing — clean up
              close(client[i].fd);
              client[i].fd = -1;
              client[i].active = 0;
              client[i].pfd.fd = -1;
            }
          }
        }
      }

      // 11. new connection enter
      // ... rest unchanged

    // 11. new connection enter
    if (fds[0].revents & POLLIN) {
      client_fd = accept(server_fd, NULL, NULL);
      for (int i=0; i<MAX_CLIENT; i++) {
        if (!client[i].active) {  // ← fix: find empty slot
          client[i].fd = client_fd;
          client[i].active = 1;
          client[i].pfd.fd = client_fd;
          client[i].pfd.events = POLLIN;

          // read client type
          ClientType t;
          read(client_fd, &t, sizeof(ClientType));
          client[i].type = t;
          break;
        }
      }
    }

    // 12. read client keys
    int index = 1;
    for (int i=0; i<MAX_CLIENT; i++) {
      if (!client[i].active) continue;

      if (fds[index].revents & POLLIN) {

        Command cmd;
        int n = read(client[i].fd, &cmd, sizeof(cmd));

        // ← THIS is where disconnect check goes (on READ, not write)
        if (n <= 0) {
          close(client[i].fd);
          client[i].fd = -1;
          client[i].active = 0;
          client[i].pfd.fd = -1;
          printf("client %d disconnected\n", i);

          // unpause if this client left while paused
          if (ctx.state.paused)
            playback_toggle(&ctx.state);
        }

        else if (cmd == CMD_PATH) {
          int pathlen = 0;
          read(client[i].fd, &pathlen, sizeof(int));
          if (pathlen > 0 && pathlen < 4096) {
            char *path = malloc(pathlen + 1);
            read(client[i].fd, path, pathlen);
            path[pathlen] = '\0';
            start_playback(path);
            free(path);
          }
        }

        else handle_key(cmd, &ctx.state);
      }
      index++;
    }
  }

  close(server_fd);
  unlink(SOCKET_PATH);
  return 0;
}
