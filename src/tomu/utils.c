#include <fcntl.h>
#include <libavformat/avformat.h>
#include <pthread.h>
#include <dbus/dbus.h>
#include <curl/curl.h>
#include <libavcodec/codec.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "macros.h"
#include "structs.h"
#include "errors.h"
#include "mpris.h"
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

void cleanUP()
{
  if (tctx.stream_ctx.is_streaming)
    streaming_cleanup(&tctx.stream_ctx);

  if (tctx.fmtCTX) {
    avformat_close_input(&tctx.fmtCTX);
    tctx.fmtCTX = NULL;
  }
  if (tctx.decoderCTX) {
    avcodec_free_context(&tctx.decoderCTX);
    tctx.decoderCTX = NULL;
  }
}

void signal_handle(int sig)
{
  (void)sig;
  _exit(0);
}

// first initilizeion
void first_init()
{
  mpris_init();
  curl_global_init(CURL_GLOBAL_ALL);

  pthread_mutex_init(&tctx.list.pt_lock, NULL);
  pthread_cond_init(&tctx.list.pt_signal, NULL);
  pthread_mutex_init(&tctx.state.lock, NULL);
  pthread_cond_init(&tctx.state.wait_cond, NULL);

  pthread_create(&tctx.dbus_s.thread, NULL, mpris_loop, NULL);
  pthread_detach(tctx.dbus_s.thread);
}
