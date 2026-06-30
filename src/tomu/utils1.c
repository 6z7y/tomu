#include <fcntl.h>
#include <libavformat/avformat.h>
#include <libavcodec/codec.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include "audio_backend.h"
#include "utils1.h"
#include "DATA.h"
#include "backend.h"
#include "mpris.h"
#include "streaming.h"
#include "utils1.h"
#include "utils2.h"

// build string and return it
char *format(const char *fmt, ...)
{
  // read args
  static char buf[256];
  va_list args;
  va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  return buf;
}

void cleanUP(){
  // First, clean up streaming if active
  if (tctx.stream_ctx.is_streaming) {
    streaming_cleanup(&tctx);
  }
  
  // Then clean up FFmpeg contexts
  if (tctx.fmtCTX) {
    avformat_close_input(&tctx.fmtCTX);
    tctx.fmtCTX = NULL;
  }
  if (tctx.codecCTX) {
    avcodec_free_context(&tctx.codecCTX);
    tctx.codecCTX = NULL;
  }
}

void sig_clean(int sig)
{
  // Clean up streaming if active
  if (tctx.stream_ctx.is_streaming) {
    streaming_stop(&tctx);
  }
  for (int i = 0; i < tctx.list.queue_count; i++) free(tctx.list.queue_lists[i]);
  free(tctx.list.queue_lists);

  cleanUP();
  _exit(0);
}


// get rand num from linux kernel
unsigned int get_rand()
{
  // 1. init varibles
  int fd;
  unsigned int n; // number without negative

  // 2. open file
  if ((fd = open("/dev/urandom", O_RDONLY)) < 0) die("can't get rand num");

  // 3. read stream
  if (read(fd, &n, sizeof(n)) != sizeof(n)) die("rand:");

  // 4. close it and return it
  close(fd);
  return n;
}


// function run command
void run_command(char *cmd)
{
  FILE *p;

  if (!(p = popen(cmd, "r"))) { 
    warn("cmd:");
  }

  pclose(p);
}

/* error handle */
void verr(const char *fmt, va_list ap)
{
	vfprintf(stderr, fmt, ap);
	if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else { fputc('\n', stderr); }
}

int warn(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	verr(fmt, ap);
    va_end(ap);
  return -1;
}

void die(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
    verr(fmt, ap);
	va_end(ap);
	exit(-1);
}
