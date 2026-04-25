#include <libavformat/avformat.h>
#include <libavcodec/codec.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "socket_utils.h"
#include "utils.h"

void sig_clean(int sig)
{
  cleanUP();
  server_socket_mode(0, &ctx.server_fd); // socket off
  _exit(0);
}

void cleanUP(){
  if (ctx.fmtCTX ) avformat_close_input(&ctx.fmtCTX);
  if (ctx.codecCTX ) avcodec_free_context(&ctx.codecCTX);
}
