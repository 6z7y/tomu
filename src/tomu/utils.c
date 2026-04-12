#include <libavformat/avformat.h>
#include <libavcodec/codec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "socket_utils.h"
#include "utils.h"
#include "../shared/share_info.h"


void sig_clean(int sig)
{
  cleanUP();
  socket_mode(0, &ctx.server_fd);
  exit(0);
}

void cleanUP(){
  if (ctx.fmtCTX ) avformat_close_input(&ctx.fmtCTX);
  if (ctx.codecCTX ) avcodec_free_context(&ctx.codecCTX);
}

// void verr(const char *fmt, va_list ap)
// {
// 	vfprintf(stderr, fmt, ap);
// 	if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
// 		fputc(' ', stderr);
// 		perror(NULL);
// 	} else {
// 		fputc('\n', stderr);
// 	}
// }
//
// void warn(const char *fmt, ...)
// {
// 	va_list ap;
// 	va_start(ap, fmt);
// 	verr(fmt, ap);
//   va_end(ap);
// }
//
// void die(const char *fmt, ...)
// {
// 	va_list ap;
// 	va_start(ap, fmt);
//   verr(fmt, ap);
// 	va_end(ap);
// 	exit(-1);
// }
