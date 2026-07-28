#include <unistd.h>

#include "structs.h"
#include "output.h"
#include "macros.h"

void playback_toggle(PlayBackContext *ctx)
{
  int paused;
  WITH_LOCK(ctx->state.lock) {
    paused = ctx->state.paused = !ctx->state.paused;
    pthread_cond_broadcast(&ctx->state.wait_cond);
  }

  if (ctx->buf) {
    if (paused) ma_device_stop(&ctx->buf->device);
    else ma_device_start(&ctx->buf->device);
  }
}

void playback_stop(PlayBackContext *ctx)
{
  WITH_LOCK(ctx->state.lock) {
    if (ctx->buf) ma_device_stop(&ctx->buf->device);
    ctx->state.skip_to_next = 0;
    ctx->state.running = 0;
    pthread_cond_broadcast(&ctx->state.wait_cond);
  }
}

void seek_playback(PlayBackContext *ctx, dbus_int64_t offset)
{
  // WITH_LOCK(state->lock) {
    if (!ctx->state.seek_request) {
      ctx->state.seek_request = 1;
      ctx->state.seek_target = offset;
    }
  // }
}

void handle_audio_seek(PlayBackContext *ctx, int *duration_time, int64_t *total_samples_played)
{
  Audio_Info *inf = &ctx->inf;
  PlaybackStatus *state = &ctx->state;
  AVFormatContext *fmtCTX= ctx->fmtCTX;
  AVCodecContext *codecCTX = ctx->decoderCTX;

  double current_sec = (double)*total_samples_played / inf->sample_rate;
  double new_position_seconds = current_sec + ((double)state->seek_target / 1000000);

  if (new_position_seconds < 0) new_position_seconds = 0;
  if (new_position_seconds > *duration_time) new_position_seconds = *duration_time;

  int64_t target_pts = (int64_t)(new_position_seconds / av_q2d(inf->audioStream->time_base));

  av_seek_frame(fmtCTX, inf->audioStream_index, target_pts, AVSEEK_FLAG_BACKWARD);
  avcodec_flush_buffers(codecCTX);

  *total_samples_played = (int64_t)(new_position_seconds * inf->sample_rate);

  audio_buffer_reset(ctx);

  state->seek_request = 0;
  state->seek_target = 0;
}


void playback_speed_defualt(PlayBackContext *ctx)
{
  // WITH_LOCK(state->lock) {
    ctx->state.speed = 1.00f;
  // }
}

void playback_speed_increase(PlayBackContext *ctx)
{
  // WITH_LOCK(state->lock) {
    ctx->state.speed += 0.05f;
    if (ctx->state.speed > 2.00f) ctx->state.speed = 2.00f;
  // }
}

void playback_speed_decrease(PlayBackContext *ctx)
{
  // WITH_LOCK(state->lock) {
    ctx->state.speed -= 0.05f;
    if (ctx->state.speed < 0.25f) ctx->state.speed = 0.25f;
  // }
}

void shuffle_toggle(PlayBackContext *ctx)
{
  // WITH_LOCK(state->lock) {
    ctx->state.shuffle = !ctx->state.shuffle;
    pthread_cond_broadcast(&ctx->state.wait_cond);
  // }
}

void playback_next_audio(PlayBackContext *ctx)
{
  WITH_LOCK(ctx->state.lock) {
    ctx->state.skip_to_next = 1;
    ctx->state.running = 0;

    pthread_cond_broadcast(&ctx->state.wait_cond);
  }
}

void playback_prev_audio(PlayBackContext *ctx)
{
  WITH_LOCK(ctx->state.lock) {
    ctx->state.skip_to_next = -1;
    ctx->state.running = 0;
    pthread_cond_broadcast(&ctx->state.wait_cond);

    // for queue can prev if need signal
    WITH_LOCK(ctx->list.pt_lock) {

      if (ctx->state.loop == LOOP_PLAYLIST)
        ctx->list.queue_index = (ctx->list.queue_index - 1 + ctx->list.queue_count) % ctx->list.queue_count;

      else {
        ctx->list.queue_index--;
        if (ctx->list.queue_index < 0) ctx->list.queue_index = 0;
      }
      pthread_cond_broadcast(&ctx->list.pt_signal);
    }
  }
}
