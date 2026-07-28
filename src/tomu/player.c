#include <pthread.h>
#include <unistd.h>

#include "../../libs/miniaudio.h"
#include "output.h"
#include "player.h"
#include "player_utils.h"
#include "decoder.h"
#include "playlist.h"
#include "macros.h"
#include "stream.h"
#include "structs.h"
#include "utils.h"

int playback_run(PlayBackContext *ctx, const char *src)
{
  av_log_set_level(AV_LOG_QUIET);

  init_playbackstatus(&ctx->state);

  // if (ctx->list.src_type == SRC_URL_RAW) {
  //   if (streaming_init_playback(ctx, src) < 0) {
  //     fprintf(stderr, "[player] Failed to initialize streaming\n");
  //     return -1;
  //   }
  // } else {
  if (get_audio_info(ctx, src) < 0) {
    cleanUP(ctx);
    return -1;
  }
  get_metadata(ctx, src);
  extract_cover(ctx);
  // }

  int capacity = (ctx->inf.sample_rate) * (ctx->inf.ch) * (ctx->inf.sample_fmt_bytes) * 2;
  if (capacity <= 0) capacity = 4096;
  ctx->buf = audio_buffer_init(capacity);
  if (!ctx->buf) {
    fprintf(stderr, "[player] Failed to allocate audio buffer\n");
    cleanUP(ctx);
    return -1;
  }

  pthread_t miniaudio_pt;
  if (pthread_create(&miniaudio_pt, NULL, miniaudio_start, ctx) != 0) {
    fprintf(stderr, "[player] Failed to create miniaudio thread\n");
    ma_device_uninit(&ctx->buf->device);
    audio_buffer_destroy(ctx->buf);
    ctx->buf = NULL;
    cleanUP(ctx);
    return -1;
  }

  run_decoder(ctx);

  pthread_join(miniaudio_pt, NULL);

  while (ctx->buf->filled != 0) {
      sleep_ms(50);
  }

  ma_device_stop(&ctx->buf->device);
  ma_device_uninit(&ctx->buf->device);

  if (ctx->buf) {
    audio_buffer_destroy(ctx->buf);
    ctx->buf = NULL;
  }

  cleanUP(ctx);
  memset(&ctx->state.metadata, 0, sizeof(Audio_Metadata));

  return 0;
}
