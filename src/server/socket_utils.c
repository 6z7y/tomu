#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "socket_utils.h"
#include "control.h"
#include "utils.h"

// socket mode ( 1 = start ), ( 0 = close )
void socket_mode(int mode, int *server_fd)
{
  unlink(SOCKET_PATH); // remove old socket file

  if (mode) {
    // 1. initlize socket protocol
    struct sockaddr_un addr = { .sun_family = AF_UNIX, .sun_path = SOCKET_PATH };
    *server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (*server_fd < 0) die("Socket:");

    // 2. bind socket to file
    if (bind(*server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) die("Bind:");

    // 3. enter listen mode
    if (listen(*server_fd, MAX_CLIENT) < 0) die("Listen:");
  }

  else close(*server_fd);
}

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
}

// when server has event see call it or make new client
void accept_new_client(int *server_fd, int *client_fd, Client_connection *client)
{
  // accept 
  *client_fd = accept(*server_fd, NULL, NULL);

  // find empty slot
  for (int i=0; i<MAX_CLIENT; i++){ // find empty slot for new client
    if (!client[i].active) { // if not active to limit
      printf("new client %d\n", i + 1);
      client[i].fd = *client_fd;
      client[i].active = 1;
      client[i].pfd.fd = *client_fd;
      client[i].pfd.events = POLLIN;

      // what client type connect?
      ClientType client_type;

      read(*client_fd, &client_type, sizeof(ClientType));
      client[i].type = client_type;
      break;
    }
  }
}


void handle_client_events(int i, int *index, struct pollfd *fds, Client_connection *client)
{
  Command cmd;
  int n = read(client[i].fd, &cmd, sizeof(cmd));

  // check disconnect on read, not write
  if (n <= 0) {
    client_die(i, client);
    printf("client %d disconnected\n", i+1);

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

void *start_playback_thread(void *arg) {
    char *path = (char*)arg;
    path_handle(path, 0, 1, 0);
    free(path);
    ctx.playback_active = 0;  // mark done when playback ends
    return NULL;
}

void start_playback(const char *path) {
    // If already playing, stop it first
    if (ctx.playback_active) {
        playback_stop(&ctx.state);
        pthread_join(ctx.playback_thread, NULL);
        ctx.playback_active = 0;
    }

    char *path_copy = strdup(path);
    pthread_create(&ctx.playback_thread, NULL, start_playback_thread, path_copy);
    ctx.playback_active = 1;
}

void write_socket(int sock, char *msg)
{
  write(sock, msg, strlen(msg));
}
