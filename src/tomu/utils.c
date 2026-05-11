#include <libavformat/avformat.h>
#include <libavcodec/codec.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "discord_integration.h"
#include "socket_utils.h"
#include "utils.h"

void sig_clean(int sig)
{
  discord_cleanup();  // ADD THIS - cleanup on Ctrl+C
  for (int i = 0; i < ctx.queue_count; i++)
    free(ctx.queue_list[i]);

  cleanUP();
  server_socket_mode(&ctx.server_fd, 0); // socket off
  _exit(0);
}

void cleanUP(){

  if (ctx.fmtCTX)  avformat_close_input(&ctx.fmtCTX);
  if (ctx.codecCTX) avcodec_free_context(&ctx.codecCTX);
}
