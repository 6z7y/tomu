#include <pthread.h>
#include <unistd.h>
#include <string.h>

#include "DATA.h"
#include "audio_backend.h"
#include "backend.h"
#include "backend_utils.h"
#include "decoder.h"

#include "../../libs/miniaudio.h"

#if LIBSWRESAMPLE_VERSION_MAJOR <= 3
  #define LEGACY_LIBSWRSAMPLE
#endif

// this handles playing audio files.
int playback_run(const char *filename, uint loop_mode, uint shuffle_mode)
{
  av_log_set_level(AV_LOG_QUIET); // ignore warning

  // 1. Read file audio
  if (get_audio_info(filename) < 0) return -1;
  get_metadata(); // get metadata
  extract_cover(filename, ctx.fmtCTX); // extract cover img 

  Command cmd = CMD_FIRST_INFOS;
  char *name_playback = ctx.list.queue_list[ctx.list.queue_index];
  uint8_t name_len = strlen(name_playback);
  for_each_num(MAX_CLIENT) if (ctx.client[i].active) {
    write(ctx.client[i].fd, &cmd,      sizeof(cmd));
    write(ctx.client[i].fd, &name_len, sizeof(name_len));
    write(ctx.client[i].fd, name_playback, name_len);
    write(ctx.client[i].fd, &ctx.state.metadata, sizeof(ctx.state.metadata));
  }

  // 2. Initialize playback status
  init_playbackstatus(&ctx.state, loop_mode, shuffle_mode);

  // initialize a buffer, size = 500ms (Streaming mode)
  int capacity = (ctx.inf.sample_rate) * (ctx.inf.ch) * (ctx.inf.sample_fmt_bytes) * 0.3;
  ctx.buf = audio_buffer_init(capacity);

  // 3. init miniaudio device
  ma_device device;
  ma_device_config ma_config = init_miniaudioConfig(&ctx.inf);

  // 4. initialize the device output
  if (ma_device_init(NULL, &ma_config, &device) != MA_SUCCESS ) goto clean_every;

  // 7. start threads
  pthread_t decoder_thread;
  pthread_create(&decoder_thread, NULL, run_decoder, NULL);
  
  // Start audio playback device
  ma_device_start(&device);

  pthread_join(decoder_thread, NULL);

clean_every:
  cmd = CMD_END;
  for_each_num(MAX_CLIENT) if (ctx.client[i].active) {
    write(ctx.client[i].fd, &cmd, sizeof(cmd));
  }

  ma_device_stop(&device);
  ma_device_uninit(&device);
  audio_buffer_destroy(ctx.buf);
  ctx.buf = NULL;
  pthread_mutex_destroy(&ctx.state.lock);
  pthread_cond_destroy(&ctx.state.wait_cond);
  cleanUP();
  // Zero out per-song state so next song starts clean
  // memset(&ctx.inf, 0, sizeof(ctx.inf));
  return 0;
}
