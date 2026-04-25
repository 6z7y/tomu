#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "socket_utils.h"
#include "control.h"
#include "../shared/share_utils.h"

// function used for write to socket
// about arg mode (1=msg, 2=Struct_Status)
static inline void write_sock(int fd, int mode, char *msgg, TomuStatus *status)
{
  if      (mode == 1) write(fd, msgg, strlen(msgg));
  else if (mode == 2) write(fd, status, sizeof(TomuStatus));
}

// function used for reading from socket
static inline void read_sock(int client_fd)
{
  char msg[256];
  int r = read(client_fd, msg, sizeof(msg) - 1);

  if (r > 0) {
    msg[r] = 0;
    printf("%s", msg);
    fflush(stdout);
  }
  else if (r == 0) {printf("client dissconnect\n"); close(client_fd);}
  else {perror("read");}
}


// function for kill client
static inline void client_die(int i, CLIENTS_SYSTEM *client)
{
  close(client[i].fd);
  client[i].fd = -1;
  client[i].active = 0;
  client[i].pfd.fd = -1;
  printf("client %d disconnected\n", i+1);
}

// add client active to poll struct
int add_client_into_poll(struct pollfd *fds, CLIENTS_SYSTEM *client)
{
  int nfds = 1; // begin from 1, because 0 for server
  for_each_num(MAX_CLIENT) {
    if (client[i].active) {
      fds[nfds] = client[i].pfd; // add client active to fds struct poll
      nfds++;
    }
  }
  return nfds; // number clients active
}

// function for send status for clients
static TomuStatus status;
void broadcast_status(CLIENTS_SYSTEM *client)
{
  if (!ctx.playback_active || !ctx.state.ready) return;

  TomuStatus status = {
    .duration  = ctx.state.duration,
    .position  = ctx.state.position,
    .paused    = ctx.state.paused,
    .volume    = ctx.state.volume,
    .speed     = ctx.state.speed,
    .shuffle   = ctx.state.shuffle,
    .loop      = ctx.state.looping,
  };

  for_each_num(MAX_CLIENT) {
    if (client[i].active) {
      write_sock(client[i].fd, 2, NULL, &status);
    }
  }
}

// when server has event see call it or make new client
void accept_new_client(int *server_fd, int *client_fd, CLIENTS_SYSTEM *client)
{
  // accept 
  *client_fd = accept(*server_fd, NULL, NULL);
  if (*client_fd < 0) return;

  int slot = 0; // check slot value

  // find empty slot
  for_each_num(MAX_CLIENT) { // find empty slot for new client
    if (!client[i].active) { // if not active to limit
      printf("new client %d type: %d\n", i + 1, client[i].type);
      client[i].fd = *client_fd;
      client[i].active = 1;
      client[i].pfd.fd = *client_fd;
      client[i].pfd.events = POLLIN;

      slot = 1;

      // what client type connect?
      ClientType client_type;

      read(*client_fd, &client_type, sizeof(ClientType));
      client[i].type = client_type;
      break;
    } else {
    }
  }

  if (!slot) { // server is full limit client
    write_sock(*client_fd, 1, "server is full!\n", NULL);
    close(*client_fd);
  }
}

// function for reading client action
static Command cmd;
static void handle_client_events(int i, struct pollfd *fds, CLIENTS_SYSTEM *client)
{

  int n = read(client[i].fd, &cmd, sizeof(cmd));

  // if client quit will read this
  if (n <= 0) {
    client_die(i, client);
    printf("test here handle_client_event()\n");
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
              printf("add %s in list %d\n", ctx.queue_list[ctx.queue_count], ctx.queue_count);
              ctx.queue_count++;
          }
          free(path);

          // only start if nothing is playing
          if (!ctx.playback_active) start_playback(ctx.queue_list[ctx.queue_index]);
      }
  }
  else {
    handle_key(cmd, &ctx.state);
  }

  broadcast_status(client);
}

void client_checker_event(int nfds, struct pollfd *fds, CLIENTS_SYSTEM *client)
{
  int index = 1; // begin from 1 for clients, server take 0
  for_each_num(MAX_CLIENT) {
    if (!client[i].active) continue;

    if (index < nfds && fds[index].revents & POLLIN) {
      handle_client_events(i, fds, client);
    }
    index++;

  }
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

void start_playback(char *path)
{
    if (ctx.playback_active) {
        playback_stop(&ctx.state);
        pthread_detach(ctx.playback_thread);
        ctx.playback_active = 0;
    }

    pthread_create(&ctx.playback_thread, NULL, start_playback_thread, path);
    ctx.playback_active = 1;
}

// socket mode ( 1 = start ), ( 0 = close )
void server_socket_mode(int mode, int *server_fd)
{
  unlink(SOCKET_PATH); // remove old socket file

  if (mode) {
    // initlize socket protocol
    struct sockaddr_un addr = { AF_UNIX, SOCKET_PATH };
    *server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (*server_fd < 0) die("Socket:");

    // bind socket to file
    if (bind(*server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) die("Bind:");

    // enter listen mode
    if (listen(*server_fd, MAX_CLIENT) < 0) die("Listen:");
  }

  else close(*server_fd);
}
