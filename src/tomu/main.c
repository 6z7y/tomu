#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <poll.h>
#include <sys/inotify.h>
#include <curl/curl.h>

#include "../../libs/miniaudio.h"
#include "args.h"
#include "decoder.h"
#include "output.h"
#include "player_utils.h"
#include "errors.h"
#include "playlist.h"
#include "structs.h"
#include "macros.h"
#include "utils.h"

// Playback Lifecycle
int playback_run(PlayBackContext *ctx, const char *src)
{
  ctx->list.src_type = extract_src_type(src); // search about source type

  // 2.1. Init default Playback Status
  init_playbackstatus(&ctx->state);

  // 2.2. extract information from src file
  if (get_audio_info(ctx, src) < 0) {
    cleanUP(ctx);
    return -1;
  }

  // 2.3. extract metadata
  get_metadata(ctx, src);

  // 2.4. extract cover
  extract_cover(ctx);

  // 2.5. init audio buffer
  ctx->buf = audio_buffer_init(ctx);
  if (!ctx->buf) {
    fprintf(stderr, "[player] Failed to allocate audio buffer\n");
    cleanUP(ctx);
    return -1;
  }

  // 2.6. init miniaudio as thread for output audio
  pthread_t miniaudio_pt;
  if (pthread_create(&miniaudio_pt, NULL, miniaudio_start, ctx) != 0) {
    fprintf(stderr, "[player] Failed to create miniaudio thread\n");
    ma_device_uninit(&ctx->buf->device);
    audio_buffer_destroy(ctx->buf);
    ctx->buf = NULL;
    cleanUP(ctx);
    return -1;
  }
  pthread_detach(miniaudio_pt);

  // 2.7. run decoder for play
  run_decoder(ctx);

  // 
  // while (ctx->buf->device_initialized && ctx->buf->filled != 0) {
  //     sleep_ms(50);
  // }


  // 2.8. stop miniaudio && clean up
  ma_device_stop(&ctx->buf->device);
  ma_device_uninit(&ctx->buf->device);
  audio_buffer_destroy(ctx->buf);
  cleanUP(ctx);

  return 0;
}

int main(int argc, char **argv)
{
  if (signal(SIGINT, signal_handle) == SIG_ERR) die("signal SIGINT:");
  if (signal(SIGTERM, signal_handle) == SIG_ERR) die("signal SIGTERM:");
  av_log_set_level(AV_LOG_QUIET); // ignore warning from ffmpeg

  // 0.1. Initialize playback context.
  PlayBackContext ctx = {0};

  // 0.2. Handle command-line arguments.
  if (argc > 1) args_handle(&ctx, argc, argv);

  // 0.3. Main playback loop.
  while(true) {
    // 1.1 Wait indefinitely if there is no music in the queue.
    while (ctx.list.queue_index >= ctx.list.queue_count)
      pthread_cond_wait(&ctx.list.pt_signal, &ctx.list.pt_lock);

    // 1.2 Automatic move queue index
    if (ctx.list.queue_index == ctx.list.queue_history_count) {
      int idx;

      if (ctx.state.shuffle) // shuffle mode
        idx = get_rand() % ctx.list.queue_count;

      else // without shuffle
        idx = ctx.list.queue_index;

      // init new space for history
      ctx.list.queue_history = realloc(ctx.list.queue_history, sizeof(int) * (ctx.list.queue_history_count + 1));

      ctx.list.queue_history[ctx.list.queue_history_count] = idx; // add
      ctx.list.queue_history_count++; // increase history count
      ctx.list.queue_index = ctx.list.queue_history_count - 1; // change index to last
    }

    const int idx = ctx.list.queue_history[ctx.list.queue_index]; // index of playback

    char *src = strdup(ctx.list.queue_lists[idx]); // take the name

    // 1.3. run playback
    playback_run(&ctx, src);

    // if next
    if (ctx.state.skip_to_next == 1) {
      if (ctx.state.shuffle) {
        if (ctx.list.queue_index + 1 < ctx.list.queue_history_count)
          ctx.list.queue_index++;
        else
          ctx.list.queue_index = ctx.list.queue_history_count;
      } else {
        if (ctx.state.loop == LOOP_PLAYLIST)
          ctx.list.queue_index = (ctx.list.queue_index + 1) % ctx.list.queue_count;
        else
          ctx.list.queue_index++;
      }
    }

    // automatic next
    else {
      if (ctx.state.loop == LOOP_PLAYLIST)
        ctx.list.queue_index = (ctx.list.queue_index + 1) % ctx.list.queue_count;
      else
        ctx.list.queue_index++;
    }

    free(src);
    ctx.state.skip_to_next = 0;
  }

  return 0;
}
