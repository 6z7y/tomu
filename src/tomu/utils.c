#include <fcntl.h>
#include <libavformat/avformat.h>
#include <pthread.h>
#include <dbus/dbus.h>
#include <curl/curl.h>
#include <libavcodec/codec.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/inotify.h>

#include "control.h"
#include "macros.h"
#include "structs.h"
#include "errors.h"
#include "stream.h"

char *format(const char *fmt, ...)
{
  static _Thread_local char buf[256];
  va_list args;
  va_start(args, fmt);
  vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);
  return buf;
}

unsigned int get_rand()
{
  int fd;
  unsigned int n;

  if ((fd = open("/dev/urandom", O_RDONLY)) < 0) die("can't get rand num");
  if (read(fd, &n, sizeof(n)) != sizeof(n)) die("rand:");
  close(fd);
  return n;
}

int run_command(char *cmd)
{
  FILE *p = popen(cmd, "r");
  if (!p) {
    warn("cmd:");
    return -1;
  }
  pclose(p);
  return 0;
}

void cleanUP(PlayBackContext *ctx)
{
  // if (ctx->stream_ctx.is_streaming)
    // streaming_cleanup(ctx->stream_ctx);

  if (ctx->fmtCTX) {
    avformat_close_input(&ctx->fmtCTX);
  }
  if (ctx->decoderCTX) {
    avcodec_free_context(&ctx->decoderCTX);
  }
}

void signal_handle(int sig)
{
  (void)sig;
  _exit(0);
}

void *read_cmd(void *arg)
{
  PlayBackContext *ctx = arg;
  char buf[2048];
  char line[256];

  int fd = inotify_init();
  if (fd < 0) {
    die("inotify_init:");
  }

  inotify_add_watch(fd, CMD_FILE, IN_CLOSE_WRITE);

  while (true) {
    ssize_t len = read(fd, &buf, sizeof(buf));
    if (len < 0) {
      warn("read:");
      return NULL;
    }

    for (size_t i=0; i<len;) {
      struct inotify_event *event = (struct inotify_event*)&buf[i];

      if (event->mask & IN_CLOSE_WRITE) {
        FILE *f = fopen(CMD_FILE, "r");

        while(fgets(line, sizeof(line), f)) {
          line[strcspn(line, "\n")] = '\0';
          printf("line: '%s'\n", line);
          if (!strcmp(line, "OPENURI")) {
            printf("here\n");

          };
          if (!strcmp(line, "PLAYPAUSE")) playback_toggle(ctx);
          if (!strcmp(line, "NEXT")) playback_next_audio(ctx);
          if (!strcmp(line, "PREV")) playback_prev_audio(ctx);
          if (!strncmp(line, "OPEN:", 5)) {
            char *path = line + 5;
          }
        }
        fclose(f);
        truncate(CMD_FILE, 0);
      }
      i += sizeof(struct inotify_event) + event->len;
    }
  }
}

void write_inf(PlayBackContext *ctx)
{
  if (!ctx->state.running) return;

  FILE *f = fopen(STATE_FILE, "w");
  if (!f) return;

  const char *status = ctx->state.paused ? "Paused" : "Playing";
  const char *loop = "None";
  if (ctx->state.loop == LOOP_TRACK)       loop = "Track";
  else if (ctx->state.loop == LOOP_PLAYLIST) loop = "Playlist";

  fprintf(f, "STATUS:%s\n", status);
  fprintf(f, "ARTIST:%s\n", ctx->state.metadata.artist);
  fprintf(f, "TITLE:%s\n", ctx->state.metadata.title);
  fprintf(f, "ALBUM:%s\n", ctx->state.metadata.album);
  fprintf(f, "ALBUM_ARTIST:%s\n", ctx->state.metadata.album_artist);
  fprintf(f, "COMPOSER:%s\n", ctx->state.metadata.composer);
  fprintf(f, "GENRE:%s\n", ctx->state.metadata.genre);
  fprintf(f, "DATE:%s\n", ctx->state.metadata.date);
  fprintf(f, "TRACK:%s\n", ctx->state.metadata.track);
  fprintf(f, "DISC:%s\n", ctx->state.metadata.disc);
  fprintf(f, "COVER:%s\n", ctx->state.metadata.cover_path);
  fprintf(f, "URL:%s\n", ctx->state.metadata.url);
  fprintf(f, "LOOP:%s\n", loop);
  fprintf(f, "DURATION:%d\n", ctx->state.duration);
  fprintf(f, "POSITION:%d\n", ctx->state.position);
  fprintf(f, "VOLUME:%.2f\n", ctx->state.volume);
  fprintf(f, "SPEED:%.2f\n", ctx->state.speed);
  fprintf(f, "SHUFFLE:%d\n", ctx->state.shuffle);

  fclose(f);
}
