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

#include "DATA.h"
#include "backend.h"

#include "../../libs/miniaudio.h"

// WRITE AUDIO DATA TO BUFFER
void audio_buffer_write(Audio_Buffer *buf, uint8_t *audio_data, int data_must_write)
{
  pthread_mutex_lock(&buf->lock);
  
  while (buf->filled + data_must_write > buf->capacity) {
    pthread_cond_wait(&buf->space_free, &buf->lock);
  }
  
  int space_until_end = buf->capacity - buf->write_pos;
  
  if (data_must_write <= space_until_end) {
    memcpy(buf->pcm_data + buf->write_pos, audio_data, data_must_write);
  } else {
    memcpy(buf->pcm_data + buf->write_pos, audio_data, space_until_end);
    
    int remaining = data_must_write - space_until_end;
    memcpy(buf->pcm_data, audio_data + space_until_end, remaining);
  }
  
  buf->write_pos = (buf->write_pos + data_must_write) % buf->capacity;
  
  buf->filled += data_must_write;
  
  pthread_cond_signal(&buf->data_ready);
  pthread_mutex_unlock(&buf->lock);
}

// READ AUDIO DATA FROM BUFFER TO SPEAKER
void audio_buffer_read(Audio_Buffer *buf, uint8_t *output, int bytes_needed)
{
  pthread_mutex_lock(&buf->lock);
  
  while (buf->filled == 0) {
    // If decoder is done and buffer is empty → write silence, don't block
    if (buf->stopped) {
      memset(output, 0, bytes_needed);
      pthread_mutex_unlock(&buf->lock);
      return;
    }
    pthread_cond_wait(&buf->data_ready, &buf->lock);
  }
  
  int bytes_to_read = bytes_needed;
  if (bytes_to_read > buf->filled) {
    bytes_to_read = buf->filled;
  }
  
  int data_until_end = buf->capacity - buf->read_pos;
  
  if (bytes_to_read <= data_until_end) {
    memcpy(output, buf->pcm_data + buf->read_pos, bytes_to_read);
  } else {
    memcpy(output, buf->pcm_data + buf->read_pos, data_until_end);
    
    int remaining = bytes_to_read - data_until_end;
    memcpy(output + data_until_end, buf->pcm_data, remaining);
  }
  
  buf->read_pos = (buf->read_pos + bytes_to_read) % buf->capacity;
  
  buf->filled -= bytes_to_read;
  
  pthread_cond_signal(&buf->space_free);
  pthread_mutex_unlock(&buf->lock);
}

// miniaudio will use this callback to read PCM samples
// this for send raw pcm audio file to speaker (send @!)
void ma_dataCallback(ma_device *ma_config, void *output, const void *input, ma_uint32 frameCount)
{
  Audio_Info *inf = &ctx.inf;
  TomuStatus *state = &ctx.state;

    // Check pause state
    pthread_mutex_lock(&state->lock);
    while (state->paused)
      pthread_cond_wait(&state->wait_cond, &state->lock);
      
    // if (state->running == false) break;
    // progress(state, current_time, duration_sec);
    pthread_mutex_unlock(&state->lock);

  
  int bytes = frameCount * inf->ch * inf->sample_fmt_bytes;
  audio_buffer_read(ctx.buf, output, bytes);

  // Apply volume
  pthread_mutex_lock(&state->lock);
  if (state->volume != 1.00f)
    ma_apply_volume_factor_pcm_frames(output, frameCount, inf->ma_fmt, inf->ch, state->volume);
  pthread_mutex_unlock(&state->lock);
}

// init miniaudio config before using
// this for init (the a sender for )
ma_device_config init_miniaudioConfig(Audio_Info *inf)
{
  ma_device_config ma_config = ma_device_config_init(ma_device_type_playback);

  ma_config.playback.channels = inf->ch;
  ma_config.playback.format = inf->ma_fmt;
  ma_config.sampleRate = inf->sample_rate;
  ma_config.dataCallback = ma_dataCallback;
  ma_config.pUserData = &ctx;

  return ma_config;
}

Audio_Buffer *audio_buffer_init(int capacity)
{
  Audio_Buffer *buf = malloc(sizeof(Audio_Buffer));

  buf->pcm_data = malloc(capacity);
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

// Reset audio buffer to empty state (used after seeking to discard old audio)
void audio_buffer_reset()
{
  pthread_mutex_lock(&ctx.buf->lock);

    ctx.buf->filled = 0;
    ctx.buf->read_pos = 0;
    ctx.buf->write_pos = 0;
    pthread_cond_broadcast(&ctx.buf->space_free);

  pthread_mutex_unlock(&ctx.buf->lock);
}

void audio_buffer_destroy(Audio_Buffer *buf)
{
  if (buf ){
    free(buf->pcm_data);
    pthread_mutex_destroy(&buf->lock);
    pthread_cond_destroy(&buf->data_ready);
    pthread_cond_destroy(&buf->space_free);
    free (ctx.buf);
  }
}

