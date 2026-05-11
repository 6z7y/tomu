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

// function used for reading from socket
// static inline void read_sock(int client_fd)
// {
//   char msg[256];
//   int r = read(client_fd, msg, sizeof(msg) - 1);
//
//   if (r > 0) {
//     msg[r] = 0;
//     printf("%s", msg);
//     fflush(stdout);
//   }
//   else if (r == 0) {printf("client dissconnect\n"); close(client_fd);}
//   else {perror("read");}
// }


// function for kill client
void client_die(int i, CLIENTS_SYSTEM *client)
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
TomuStatus status;
void broadcast_status(CLIENTS_SYSTEM *client)
{
  if (!ctx.playback_active) return;

  status.duration  = ctx.state.duration;
  status.position  = ctx.state.position;
  status.paused    = ctx.state.paused;
  status.volume    = ctx.state.volume;
  status.speed     = ctx.state.speed;
  status.shuffle   = ctx.state.shuffle;
  status.loop      = ctx.state.looping;
  status.playback_running = ctx.state.running;

  for_each_num(MAX_CLIENT) {
    if (client[i].active) {
      write_now_struct(client[i].fd, &status, sizeof(status));
    }
  }
}

// when server has event see call it or make new client
void accept_new_client(int *server_fd, int *client_fd, CLIENTS_SYSTEM *client)
{
  // accept new client or quit if not there
  if ((*client_fd = accept(*server_fd, NULL, NULL)) < 0) return;

  int slot = 0; // check slot value

  // find empty slot
  for_each_num(MAX_CLIENT) { // find empty slot for new client
    if (!client[i].active) { // if not active to limit
      printf("new client %d type: %d\n", i + 1, client[i].type);
      client[i].active = 1;
      client[i].fd = *client_fd;
      client[i].pfd.fd = *client_fd;
      client[i].pfd.events = POLLIN;

      slot = 1;

      // what client type connect?
      ClientType client_type;

      read(*client_fd, &client_type, sizeof(ClientType));
      client[i].type = client_type;
      break;
    } 
  }

  if (!slot) { // server is full limit client
    write_now_normal_msg(*client_fd, "server is full!\n");
    close(*client_fd);
  }
}

// function for reading client action
static Command cmd;
void handle_client_events(int i, struct pollfd *fds, CLIENTS_SYSTEM *client)
{
  int n = read(client[i].fd, &cmd, sizeof(cmd));

  // if client quit will read this
  if (n <= 0) {
    client_die(i, client);
    printf("test here handle_client_event()\n");
    return;  // ← return early, don't fall through
  }

  if (cmd == CMD_PATH) {
    size_t pathlen = 0;
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

  else handle_key(cmd, &ctx.state);

  broadcast_status(client);
}

void *start_playback_thread(void *arg) {
    char *path = (char*)arg;
    playback_run(path, 0, 1);
    free(path); // safe — this is always a strdup copy, not queue_list[i] directly

    ctx.playback_active = 0;

    // respect skip_to_next direction (next/prev key)
    int skip = ctx.state.skip_to_next;
    ctx.state.skip_to_next = 0;

    if (skip == -1) {
        if (ctx.queue_index > 0) ctx.queue_index--;
    } else {
        ctx.queue_index++;
    }

    if (ctx.queue_index < ctx.queue_count) {
        char *next = strdup(ctx.queue_list[ctx.queue_index]);
        pthread_t t;
        pthread_create(&t, NULL, start_playback_thread, next);
        pthread_detach(t);
        ctx.playback_thread = t;
        ctx.playback_active = 1;
    }
    // else: queue exhausted, stay idle
    return NULL;
}

void start_playback(char *path) {
    // if (ctx.playback_active) {
    //     playback_stop(&ctx.state);
    //     pthread_detach(ctx.playback_thread); // wait cleanly, don't detach
    //     ctx.playback_active = 0;
    // }
    char *copy = strdup(path); // thread owns this copy, safe to free
    pthread_create(&ctx.playback_thread, NULL, start_playback_thread, copy);
    pthread_detach(ctx.playback_thread); // wait cleanly, don't detach
    ctx.playback_active = 1;
}

// socket mode ( 1 = start ), ( 0 = close )
void server_socket_mode(int *server_fd, int ON)
{
  unlink(SOCKET_PATH); // remove old socket file

  if (ON) {
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
