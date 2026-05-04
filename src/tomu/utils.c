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
  for (int i = 0; i < ctx.queue_count; i++)
    free(ctx.queue_list[i]);

  cleanUP();
  server_socket_mode(0, &ctx.server_fd); // socket off
  _exit(0);
}

void cleanUP(){
  Audio_Metadata *m = &ctx.state.metadata;
  free(m->title);   free(m->artist); free(m->album);
  free(m->album_artist); free(m->genre);
  free(m->date);    free(m->track);
  memset(m, 0, sizeof(*m));  // reset pointers to NULL

  if (ctx.fmtCTX)  avformat_close_input(&ctx.fmtCTX);
  if (ctx.codecCTX) avcodec_free_context(&ctx.codecCTX);
}
