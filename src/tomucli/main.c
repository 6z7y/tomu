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
#include "../shared/share_utils.h"
#include "CLIENT_DATA.h"
#include "args.h"
#include "backend.h"
#include "config.h"
#include "control.h"
#include "file_handle.h"
#include "utils.h"

Client_CTX ctx = {0};

void parse_json(const char *key)
{
  TomuStatus *s = &ctx.status;
  
  // Parse status section
  if (strstr(key, "\"duration\"")) {
      char *p = strstr(key, "\"duration\"");
      p = strchr(p, ':');
      p++;
      s->duration = atoi(p);
  }
  
  if (strstr(key, "\"position\"")) {
      char *p = strstr(key, "\"position\"");
      p = strchr(p, ':');
      p++;
      s->position = atoi(p);
  }
  
  if (strstr(key, "\"paused\"")) {
      char *p = strstr(key, "\"paused\"");
      p = strchr(p, ':');
      p++;
      s->paused = atoi(p);
  }
  
  if (strstr(key, "\"volume\"")) {
      char *p = strstr(key, "\"volume\"");
      p = strchr(p, ':');
      p++;
      s->volume = atof(p);
  }
  
  if (strstr(key, "\"speed\"")) {
      char *p = strstr(key, "\"speed\"");
      p = strchr(p, ':');
      p++;
      s->speed = atof(p);
  }
  
  if (strstr(key, "\"shuffle\"")) {
      char *p = strstr(key, "\"shuffle\"");
      p = strchr(p, ':');
      p++;
      s->shuffle = atoi(p);
  }
  
  if (strstr(key, "\"loop\"")) {
      char *p = strstr(key, "\"loop\"");
      p = strchr(p, ':');
      p++;
      s->looping = atoi(p);
  }
  
  if (strstr(key, "\"playback_running\"")) {
      char *p = strstr(key, "\"playback_running\"");
      p = strchr(p, ':');
      p++;
      s->running = atoi(p);
  }
  
  // Parse metadata section (always parse regardless of status section)
  char *p;
  
  // TITLE
  if ((p = strstr(key, "\"title\""))) {
      p = strchr(p, ':');
      p++;
      p++;  // skip opening quote
      int i = 0;
      while (*p && *p != '"' && i < 255) {
          s->metadata.title[i++] = *p++;
      }
      s->metadata.title[i] = '\0';
  }
  
  // ARTIST
  if ((p = strstr(key, "\"artist\""))) {
      p = strchr(p, ':');
      p++;
      p++;
      int i = 0;
      while (*p && *p != '"' && i < 255) {
          s->metadata.artist[i++] = *p++;
      }
      s->metadata.artist[i] = '\0';
  }
  
  // ALBUM
  if ((p = strstr(key, "\"album\""))) {
      p = strchr(p, ':');
      p++;
      p++;
      int i = 0;
      while (*p && *p != '"' && i < 255) {
          s->metadata.album[i++] = *p++;
      }
      s->metadata.album[i] = '\0';
  }
  
  // ALBUM ARTIST
  if ((p = strstr(key, "\"album_artist\""))) {
      p = strchr(p, ':');
      p++;
      p++;
      int i = 0;
      while (*p && *p != '"' && i < 255) {
          s->metadata.album_artist[i++] = *p++;
      }
      s->metadata.album_artist[i] = '\0';
  }
  
  // GENRE
  if ((p = strstr(key, "\"genre\""))) {
      p = strchr(p, ':');
      p++;
      p++;
      int i = 0;
      while (*p && *p != '"' && i < 255) {
          s->metadata.genre[i++] = *p++;
      }
      s->metadata.genre[i] = '\0';
  }
  
  // DATE
  if ((p = strstr(key, "\"date\""))) {
      p = strchr(p, ':');
      p++;
      p++;
      int i = 0;
      while (*p && *p != '"' && i < 255) {
          s->metadata.date[i++] = *p++;
      }
      s->metadata.date[i] = '\0';
  }
  
  // TRACK
  if ((p = strstr(key, "\"track\""))) {
      p = strchr(p, ':');
      p++;
      p++;
      int i = 0;
      while (*p && *p != '"' && i < 255) {
          s->metadata.track[i++] = *p++;
      }
      s->metadata.track[i] = '\0';
  }
  
  // printf("dur = %d pos= %d\n", s->duration, s->position);
  // printf("title:%s\nartist:%s\nalbum:%s\n", 
  //        s->metadata.title, s->metadata.artist, s->metadata.album);
}

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
  char json[512];
  char key[8];
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
      n = read(STDIN_FILENO, key, sizeof(key) - 1);
      key[n] = '\0';
      handle_control(server_fd, key, n);
    }

    // Server status update
    if (fds[1].revents & POLLIN) {
        int n = read(*server_fd, json, sizeof(json));
        if (n > 0) {
          json[n] = '\0';
          parse_json(json);
        } 
        else die("server: dissconnected");

        TomuStatus *s = &ctx.status;
        parse_json(json);
        if (s) {
            if (status->running)
                waiting_for_playback = 0;  // server confirmed it started playing
            if (!update_status(*server_fd, &was_playing, status, queue)) {
                ctx.running = 0;
                continue;
            }
        }
        else die("server: Disconnected");
    }

    // Send next file only when idle AND not already waiting
    if (!status->running && queue->has_queue 
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
