#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <poll.h>
#include <unistd.h>
#include <pthread.h>

#include "backend_utils.h"
#include "control.h"
#include "utils.h"

static int playback_active = 0;
static pthread_t playback_thread;

typedef struct {
  int fd;
  int active;
  struct pollfd pfd;
} Client_connection;

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
    printf("Started playback: %s\n", path);
}

int main(int argc, char **argv)
{
  // See what the user wants with "--" and handle it
  if ( argv[1][0] == '-' && argv[1][1] == '-' ){

    if ( strcmp(argv[1], "--version") == 0 ){
      printf("Tomu: %s\n", SERVER_VER);
    }

    else {
      printf("[T]: unknown option '%s'\n", argv[1]);
    }
    return 0;
  }

  // 1. unlink old socket file
  unlink(SOCKET_PATH);
  int server_fd, client_fd;

  // 2. create server socket
  struct sockaddr_un addr = { AF_UNIX, SOCKET_PATH };
  server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
  if (server_fd < 0) die("Socket:");

  // 3. bind
  if (bind(server_fd, (const struct sockaddr*)&addr, sizeof(addr)) < 0) die("Bind:");

  // 4. listen
  if (listen(server_fd, MAX_CLIENT) < 0) die("Listen:");

  // 5. client array
  Client_connection client[MAX_CLIENT] = {0};

  struct pollfd fds[1 + MAX_CLIENT];

  // 7. main loop
  while(1) {
    fds[0].fd = server_fd;
    fds[0].events = POLLIN;

    int nfds = 1;
    for (int i = 0; i < MAX_CLIENT; i++) {
      if (client[i].active) {
        fds[nfds] = client[i].pfd;
        nfds++;
      }
    }

    int ret = poll(fds, nfds, 1000);  // wake up every 1 second

    if (ret == 0) {  // timeout = 1 second passed, no key pressed
      if (playback_active) {
        int pos = ctx.state.position;
        int dur = ctx.state.duration > 0 ? ctx.state.duration : 1;
        int bar_width = 30;
        int bar_pos = pos * bar_width / dur;
        char bar[32];
        for (int b = 0; b < bar_width; b++)
          bar[b] = b < bar_pos ? '=' : (b == bar_pos ? '>' : '.');
        bar[bar_width] = '\0';
        char msg[256];
        snprintf(msg, sizeof(msg), "\r%s[%s] %02d:%02d/%02d:%02d vol:%.0f%% spd:%.2fx",
                 ctx.state.paused ? "(Paused) " : "", bar,
                 get_min(pos), get_sec(pos), get_min(dur), get_sec(dur),
                 ctx.state.volume * 100, ctx.state.speed);
        for (int i = 0; i < MAX_CLIENT; i++)
          if (client[i].active)
            write(client[i].fd, msg, strlen(msg));
      }
      continue;
    }

    if (ret < 0) { warn("POLL ret:"); continue; }

    // new connection?
    if (fds[0].revents & POLLIN) {
      client_fd = accept(server_fd, NULL, NULL);
      if (client_fd <= 0) { warn("Connection:"); continue; }
      printf("new connection\n");

      for (int i = 0; i < MAX_CLIENT; i++) {
        if (!client[i].active) {
          client[i].fd = client_fd;
          client[i].active = 1;
          client[i].pfd.fd = client_fd;
          client[i].pfd.events = POLLIN;
          write(client[i].fd, "ok", 2);
          break;
        }
      }
    }

    // read from clients
    int index = 1;
    for (int i = 0; i < MAX_CLIENT; i++) {
      if (!client[i].active) continue;

      if (fds[index].revents & POLLIN) {

        // --- read command (4 bytes) ---
        Command cmd;
        int n = read(client[i].fd, &cmd, sizeof(Command));

        if (n <= 0) {
          close(client[i].fd);
          client[i].fd = -1;
          client[i].active = 0;
          client[i].pfd.fd = -1;
          printf("client %d disconnected\n", i);

        } else if (cmd == CMD_PATH) {
          // --- CMD_PATH: next read is the path string ---
          // Protocol: CMD_PATH (4 bytes) + path length (4 bytes) + path string
          int pathlen = 0;
          read(client[i].fd, &pathlen, sizeof(int));

          if (pathlen > 0 && pathlen < 4096) {
            char *path = malloc(pathlen + 1);
            read(client[i].fd, path, pathlen);
            path[pathlen] = '\0';
            printf("client %d wants to play: %s\n", i+1, path);
            start_playback(path);
            free(path);
          }

        } else {
          // normal command (pause, seek, volume...)
          printf("client %d cmd: %d\n", i+1, cmd);
          handle_key(cmd, &ctx.state);

          // status
          // reply with status
          char msg[256];
          snprintf(msg, sizeof(msg), "pos:%d dur:%d vol:%.0f%% speed:%.2fx\n",
                   ctx.state.position, ctx.state.duration,
                   ctx.state.volume * 100, ctx.state.speed);
          write(client[i].fd, msg, strlen(msg));
        }
      }
      index++;
    }
  }

  close(server_fd);
  unlink(SOCKET_PATH);
  return 0;
}
