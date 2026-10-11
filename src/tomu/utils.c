#include <errno.h>
#include <libavformat/avformat.h>
#include <pthread.h>
#include <curl/curl.h>
#include <libavcodec/codec.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/inotify.h>
#include <fcntl.h>
#include <sys/file.h>

// #include "control.h"
#include "control.h"
#include "macros.h"
#include "playlist.h"
#include "structs.h"
#include "errors.h"

int single_instance() {
  int fd = open("/tmp/tomu.lock", O_CREAT | O_RDWR, 0644);

  if (flock(fd, LOCK_EX | LOCK_NB) < 0) {
    return -1;
  }

  return 0;
}

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

  memset(&ctx->state.metadata, 0, sizeof(Audio_Metadata));
}

void signal_handle(int sig)
{
  (void)sig;
  _exit(0);
}

void *read_cmd(void *arg)
{
  PlayBackContext *ctx = arg;

  // create fifo cmd file
  int ffd = open(CMD_FILE, O_CREAT | O_WRONLY, 0644);
  if (ffd >= 0) close(ffd);

  char buf[2048];
  char line[256];

  int fd = inotify_init();
  if (fd < 0) {
    die("inotify_init:");
  }

  inotify_add_watch(fd, CMD_FILE, IN_CLOSE_WRITE);

  while (true) {
    ssize_t len = read(fd, buf, sizeof(buf));
    if (len < 0) {
      if (errno == EINTR) continue;// todo: practies errno
      warn("read:");
      return NULL;
    }

    for (size_t i=0; i< (size_t)len;) {
      struct inotify_event *event = (struct inotify_event*)&buf[i];

      if (event->mask & IN_CLOSE_WRITE) {
        FILE *f = fopen(CMD_FILE, "r");
        int had = 0;
        if (f) {
          while(fgets(line, sizeof(line), f)) {
            line[strcspn(line, "\n")] = '\0';
            if (!line[0]) continue;
            had = 1;
            printf("line: '%s'\n", line);

              if      (!strcmp(line, "PLAYPAUSE"))  playback_toggle(ctx);
              else if (!strcmp(line, "PLAY"))       ctx->state.paused = 0;
              else if (!strcmp(line, "PAUSE"))      ctx->state.paused = 1;
              else if (!strcmp(line, "STOP"))       playback_stop(ctx);
              else if (!strcmp(line, "NEXT"))       playback_next_audio(ctx);
              else if (!strcmp(line, "PREV"))       playback_prev_audio(ctx);
              else if (!strncmp(line, "OPEN:", 5))  src_handle(ctx, line + 5);
              // else if (!strncmp(line, "SETVOLUME:", 10))  playback_set_volume(ctx, atof(line + 10));
              // else if (!strncmp(line, "SETLOOP:", 8))     playback_set_loop(ctx, line + 8);
              // else if (!strncmp(line, "SETSHUFFLE:", 11)) playback_set_shuffle(ctx, atoi(line + 11));
              // else if (!strncmp(line, "SETSPEED:", 9))    playback_set_speed(ctx, atof(line + 9));
              // else if (!strncmp(line, "SEEK:", 5))        playback_seek(ctx, atol(line + 5));
              // else if (!strncmp(line, "SETPOS:", 7))      playback_set_position(ctx, atol(line + 7));
          }
        }
        rewind(f);
        ftruncate(fileno(f), 0);
        fclose(f);
        if (had) {
          int t = open(CMD_FILE, O_WRONLY | O_TRUNC);
          if (t >= 0) close(t);
        }
      }
      i += sizeof(struct inotify_event) + event->len;
    }
  }
}
