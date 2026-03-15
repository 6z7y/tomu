#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "socket_utils.h"
#include "control.h"

// add client active to poll struct
int add_client_into_poll(struct pollfd *fds, Client_connection *client)
{
  int nfds = 1; // begin from 1, because 0 for server
  for (int i=0; i<MAX_CLIENT; i++) {
    if (client[i].active) {
      fds[nfds] = client[i].pfd; // add client active to fds struct poll
      nfds++; // continue to limit MAX_CLIENT
    }
  }
  return nfds; // number clients active
}

void broadcast_status(Client_connection *client)
{
  if (!ctx.playback_active || !ctx.state.ready) return;  // ← add this

  TomuStatus status = {
    .duration  = ctx.state.duration,
    .position  = ctx.state.position,
    .paused    = ctx.state.paused,
    .volume    = ctx.state.volume,
    .speed     = ctx.state.speed,
    .shuffle   = ctx.state.shuffle,
    .loop      = ctx.state.looping,
  };

  for (int i=0; i<MAX_CLIENT; i++) {
    if (client[i].active) {
      if (write(client[i].fd, &status, sizeof(status)) <= 0) {
        // client died while writing — clean up
        printf("jhhhhhhhhhh here in broadcaststatus\n");
        client_die(i, client);
      }
    }
  }
}

void client_die(int i, Client_connection *client)
{

  close(client[i].fd);
  client[i].fd = -1;
  client[i].active = 0;
  client[i].pfd.fd = -1;
  printf("client %d disconnected\n", i+1);
}

// when server has event see call it or make new client
void accept_new_client(int *server_fd, int *client_fd, Client_connection *client)
{
  // accept 
  *client_fd = accept(*server_fd, NULL, NULL);

  // find empty slot
  for (int i=0; i<MAX_CLIENT; i++){ // find empty slot for new client
    if (!client[i].active) { // if not active to limit
      printf("new client %d type: %d\n", i + 1, client[i].type);
      client[i].fd = *client_fd;
      client[i].active = 1;
      client[i].pfd.fd = *client_fd;
      client[i].pfd.events = POLLIN;

      // what client type connect?
      ClientType client_type;

      read(*client_fd, &client_type, sizeof(ClientType));
      client[i].type = client_type;
      break;
    } else {
      printf("server is full!\n");
      // Command cmd = CMD_QUIT_SERVER_FULL;
      // send_cmd(client[i].fd, cmd);
      // close(client[i].fd);
    }
  }
}


void handle_client_events(int i, struct pollfd *fds, Client_connection *client)
{
  Command cmd;
  int n = read(client[i].fd, &cmd, sizeof(cmd));

  if (n <= 0) {
    client_die(i, client);
    if (ctx.state.paused)
      playback_toggle(&ctx.state);
    return;  // ← return early, don't fall through
  }

  if (cmd == CMD_PATH) {
      int pathlen = 0;
      read(client[i].fd, &pathlen, sizeof(int));
      if (pathlen > 0 && pathlen < 2048) {
          char *path = malloc(pathlen + 1);
          read(client[i].fd, path, pathlen);
          path[pathlen] = '\0';

          if (ctx.queue_count < 200) {
              ctx.queue_list[ctx.queue_count] = strdup(path);
              ctx.queue_count++;
          }
          free(path);

          // only start if nothing is playing
          if (!ctx.playback_active)
              start_playback(ctx.queue_list[ctx.queue_index]);
      }
  }
  else {
    handle_key(cmd, &ctx.state);
  }

  broadcast_status(client);
}

void *start_playback_thread(void *arg) {
    char *path = (char*)arg;
    playback_run(path, 0, 1);
    free(path);
    ctx.playback_active = 0;

    ctx.queue_index++;
    if (ctx.queue_index < ctx.queue_count) {
        char *next = strdup(ctx.queue_list[ctx.queue_index]);
        pthread_create(&ctx.playback_thread, NULL, start_playback_thread, next);
        ctx.playback_active = 1;
    }

    return NULL;
}

void start_playback(const char *path) {
    if (ctx.playback_active) {
        playback_stop(&ctx.state);
        pthread_detach(ctx.playback_thread);  // ← detach instead of join
        ctx.playback_active = 0;
    }

    char *path_copy = strdup(path);
    pthread_create(&ctx.playback_thread, NULL, start_playback_thread, path_copy);
    ctx.playback_active = 1;
}

// void write_socket(int sock, char *msg)
// {
//   write(sock, msg, strlen(msg));
// }
