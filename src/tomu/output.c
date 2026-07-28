#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <stdlib.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "structs.h"
#include "player.h"
#include "utils.h"

#include "../../libs/miniaudio.h"

void audio_buffer_write(Audio_Buffer *buf, uint8_t *audio_data, int data_must_write)
{
  pthread_mutex_lock(&buf->lock);

  if (data_must_write <= 0 || (size_t)data_must_write > buf->capacity) {
    pthread_mutex_unlock(&buf->lock);
    return;
  }

  while (buf->filled + data_must_write > (int)buf->capacity)
    pthread_cond_wait(&buf->space_free, &buf->lock);

  int space_until_end = (int)buf->capacity - (int)buf->write_pos;

  if (data_must_write <= space_until_end)
    memcpy(buf->pcm_data + buf->write_pos, audio_data, data_must_write);
  else {
    memcpy(buf->pcm_data + buf->write_pos, audio_data, space_until_end);

    int remaining = data_must_write - space_until_end;
    memcpy(buf->pcm_data, audio_data + space_until_end, remaining);
  }

  buf->write_pos = (buf->write_pos + data_must_write) % buf->capacity;
  buf->filled += data_must_write;

  pthread_cond_signal(&buf->data_ready);
  pthread_mutex_unlock(&buf->lock);
}

void audio_buffer_read(Audio_Buffer *buf, uint8_t *output, int bytes_needed)
{
  pthread_mutex_lock(&buf->lock);

  while (buf->filled == 0) {
    if (buf->stopped) {
      memset(output, 0, bytes_needed);
      pthread_mutex_unlock(&buf->lock);
      return;
    }
    pthread_cond_wait(&buf->data_ready, &buf->lock);
  }

  int bytes_to_read = bytes_needed;
  if (bytes_to_read > buf->filled)
    bytes_to_read = buf->filled;

  int data_until_end = (int)buf->capacity - (int)buf->read_pos;

  if (bytes_to_read <= data_until_end)
    memcpy(output, buf->pcm_data + buf->read_pos, bytes_to_read);
  else {
    memcpy(output, buf->pcm_data + buf->read_pos, data_until_end);

    int remaining = bytes_to_read - data_until_end;
    memcpy(output + data_until_end, buf->pcm_data, remaining);
  }

  buf->read_pos = (buf->read_pos + bytes_to_read) % buf->capacity;
  buf->filled -= bytes_to_read;

  pthread_cond_signal(&buf->space_free);
  pthread_mutex_unlock(&buf->lock);
}

void ma_dataCallback(ma_device *ma_config, void *output, const void *input, ma_uint32 frameCount)
{
  (void)input;
  PlayBackContext *ctx = ma_config->pUserData; 

  Audio_Info *inf = &ctx->inf;
  PlaybackStatus *state = &ctx->state;

  pthread_mutex_lock(&state->lock);
  while (state->paused)
    pthread_cond_wait(&state->wait_cond, &state->lock);
  pthread_mutex_unlock(&state->lock);

  int bytes = frameCount * inf->ch * inf->sample_fmt_bytes;
  audio_buffer_read(ctx->buf, output, bytes);

  pthread_mutex_lock(&state->lock);
  if (state->volume != 1.00f)
    ma_apply_volume_factor_pcm_frames(output, frameCount, inf->ma_fmt, inf->ch, state->volume);
  pthread_mutex_unlock(&state->lock);
}

ma_device_config init_miniaudioConfig(PlayBackContext *ctx)
{
  ma_device_config ma_config = ma_device_config_init(ma_device_type_playback);

  ma_config.playback.channels = ctx->inf.ch;
  ma_config.playback.format = ctx->inf.ma_fmt;
  ma_config.sampleRate = ctx->inf.sample_rate;
  ma_config.dataCallback = ma_dataCallback;
  ma_config.pUserData = ctx;

  return ma_config;
}

Audio_Buffer *audio_buffer_init(int capacity)
{
  Audio_Buffer *buf = malloc(sizeof(Audio_Buffer));
  if (!buf) return NULL;

  buf->pcm_data = malloc(capacity > 0 ? capacity : 1);
  buf->capacity = capacity;
  buf->write_pos = 0;
  buf->read_pos = 0;
  buf->filled = 0;
  buf->stopped = 0;

  pthread_mutex_init(&buf->lock, NULL);
  pthread_cond_init(&buf->data_ready, NULL);
  pthread_cond_init(&buf->space_free, NULL);

  return buf;
}

void audio_buffer_reset(PlayBackContext *ctx)
{
  pthread_mutex_lock(&ctx->buf->lock);
  ctx->buf->filled = 0;
  ctx->buf->read_pos = 0;
  ctx->buf->write_pos = 0;
  pthread_cond_broadcast(&ctx->buf->space_free);
  pthread_mutex_unlock(&ctx->buf->lock);
}

void audio_buffer_destroy(Audio_Buffer *buf)
{
  if (!buf) return;

  free(buf->pcm_data);
  pthread_mutex_destroy(&buf->lock);
  pthread_cond_destroy(&buf->data_ready);
  pthread_cond_destroy(&buf->space_free);
  free(buf);
}

void *miniaudio_start(void *arg)
{
  PlayBackContext *ctx = arg;

  ma_device_config ma_config = init_miniaudioConfig(ctx);

  if (ma_device_init(NULL, &ma_config, &ctx->buf->device) != MA_SUCCESS) {
    fprintf(stderr, "[player] Failed to initialize miniaudio\n");
    audio_buffer_destroy(ctx->buf);
    ctx->buf = NULL;
    cleanUP(ctx);
    printf("problem in mini audio start\n");
    exit(1); // for test if there problem
  }

  if (ma_device_start(&ctx->buf->device) != MA_SUCCESS) {
    fprintf(stderr, "[player] Failed to start audio device\n");
    ctx->state.running = 0;
    ma_device_uninit(&ctx->buf->device);
    audio_buffer_destroy(ctx->buf);
  }

  return NULL;
}
