#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "socket_utils.h"
#include "DATA.h"
#include "control.h"
#include "../shared/share_utils.h"

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
// TomuStatus status;
static void create_json_fmt(char *msg, size_t msg_size)
{
  TomuStatus *s = &ctx.state;
  snprintf(msg, msg_size,
      "{"
      "  \"status\": {"
      "    \"duration\":%d,"
      "    \"position\":%d,"
      "    \"paused\":%d,"
      "    \"volume\":%.1f,"
      "    \"speed\":%.2f,"
      "    \"shuffle\":%d,"
      "    \"loop\":%d,"
      "    \"playback_running\":%d"
      "  },"
      "  \"metadata\": {"
      "    \"title\":\"%s\","
      "    \"artist\":\"%s\","
      "    \"album\":\"%s\","
      "    \"album_artist\":\"%s\","
      "    \"genre\":\"%s\","
      "    \"date\":\"%s\","
      "    \"track\":\"%s\""
      "  }"
      "}",
      s->duration, s->position, s->paused, s->volume, s->speed, 
      s->shuffle, s->looping, s->running,
      s->metadata.title, s->metadata.artist, s->metadata.album, 
      s->metadata.album_artist, s->metadata.genre, s->metadata.date, s->metadata.track
  );
}

void broadcast_status(CLIENTS_SYSTEM *client)
{
  if (!ctx.playback_active) return;

  char json_msg[1024];
  create_json_fmt(json_msg, sizeof(json_msg));  // Pass buffer size
  
  size_t len = strlen(json_msg) + 1;
  
  for_each_num(MAX_CLIENT) {
    if (client[i].active) {
      write(client[i].fd, json_msg, len);
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
    if (pathlen > 0) {
      char *path = malloc(pathlen + 1);
      read(client[i].fd, path, pathlen);
      path[pathlen] = '\0';

      if (ctx.queue_count < 200) {
          ctx.queue_list[ctx.queue_count] = strdup(path);
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
  if (ON) {
    char socket_path[512];

    // initlize socket protocol
    struct sockaddr_un addr = { AF_UNIX, SOCKET_PATH };

    // test if tomu running
    int test_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (connect(test_fd, (struct sockaddr*)&addr, sizeof(addr)) == 0) {
      close(test_fd);
      printf("tomu: already running\n");
      exit(0);
    }
    close(test_fd);

    unlink(SOCKET_PATH); // remove old socket file

    // init protocol socket
    *server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (*server_fd < 0) die("Socket:");

    // bind socket to file
    if (bind(*server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) die("Bind:");

    // enter listen mode
    if (listen(*server_fd, MAX_CLIENT) < 0) die("Listen:");
  }

  else {
    unlink(SOCKET_PATH); // remove old socket file

    close(*server_fd);
  }
}
