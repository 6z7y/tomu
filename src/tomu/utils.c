#include <libavformat/avformat.h>
#include <libavcodec/codec.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "utils.h"
#include "DATA.h"
#include "streaming.h"

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
