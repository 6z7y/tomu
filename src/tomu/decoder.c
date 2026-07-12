#include <pthread.h>
#include <unistd.h>

#include "macros.h"
#include "structs.h"
#include "player_utils.h"
#include "output.h"

#include "../../libs/miniaudio.h"

void *run_decoder(void *arg)
{
  (void)arg;
  AVFormatContext *fmtCTX = tctx.fmtCTX;
  AVCodecContext *decoderCTX = tctx.decoderCTX;
  Audio_Info *inf = &tctx.inf;
  PlaybackStatus *state = &tctx.state;

  SwrContext *swrCTX = NULL;
  SwrContext *speed_swrCTX = NULL;

  if (av_sample_fmt_is_planar(decoderCTX->sample_fmt)) {
    setup_sample_fmt_resampler(inf, &swrCTX);
    if (swrCTX) {
      if (swr_init(swrCTX) < 0)
        swr_free(&swrCTX);
    }
  }

  AVPacket *packet = av_packet_alloc();
  AVFrame *frame = av_frame_alloc();

  if (!packet || !frame) {
    printf("ERROR: Failed to allocate packet/frame\n");
    av_packet_free(&packet);
    av_frame_free(&frame);
    return NULL;
  }

  int64_t total_samples_played = 0;
  int duration_sec = (fmtCTX->duration > 0) ? (int)(fmtCTX->duration / AV_TIME_BASE) : 0;
  tctx.state.duration = duration_sec;

  pthread_mutex_lock(&state->lock);
  float last_speed = state->speed;
  pthread_mutex_unlock(&state->lock);

decode:
  while (av_read_frame(fmtCTX, packet) >= 0) {
    if (packet->stream_index == inf->audioStream_index) {
      if (avcodec_send_packet(decoderCTX, packet) < 0) {
        av_packet_unref(packet);
        continue;
      }

      while (avcodec_receive_frame(decoderCTX, frame) >= 0) {
        double current_time = (double)total_samples_played / inf->sample_rate;

        pthread_mutex_lock(&state->lock);
        tctx.state.position = (int)current_time;

        if (state->seek_request) {
          handle_audio_seek(&duration_sec, &total_samples_played);
          pthread_mutex_unlock(&state->lock);
          av_packet_unref(packet);
          av_frame_unref(frame);
          goto decode;
        }

        float current_speed = state->speed;
        pthread_mutex_unlock(&state->lock);

        total_samples_played += frame->nb_samples;

        if (current_speed != last_speed) {
          last_speed = current_speed;
          if (speed_swrCTX) {
            swr_free(&speed_swrCTX);
            speed_swrCTX = NULL;
          }
          if (current_speed != 1.0f)
            setup_speed_resampler(inf, frame, &speed_swrCTX);
        }

        pthread_mutex_lock(&state->lock);
        while (state->paused)
          pthread_cond_wait(&state->wait_cond, &state->lock);
        if (!state->running) {
          pthread_mutex_unlock(&state->lock);
          av_frame_unref(frame);
          goto done;
        }
        pthread_mutex_unlock(&state->lock);

        uint8_t *output_data = NULL;
        int output_bytes = 0;

        if (speed_swrCTX) {
          int out_samples = (int)(frame->nb_samples / current_speed);
          output_bytes = out_samples * inf->ch * inf->sample_fmt_bytes;
          output_data = malloc(output_bytes);

          if (output_data) {
            uint8_t *data_out[1] = {output_data};
            int samples = swr_convert(speed_swrCTX, data_out, out_samples,
                                     (const uint8_t**)frame->data, frame->nb_samples);
            if (samples > 0)
              output_bytes = samples * inf->ch * inf->sample_fmt_bytes;
            else {
              free(output_data);
              output_data = NULL;
            }
          }
        } else if (swrCTX) {
          output_bytes = frame->nb_samples * inf->ch * inf->sample_fmt_bytes;
          output_data = malloc(output_bytes);

          if (output_data) {
            uint8_t *data[1] = {output_data};
            int samples = swr_convert(swrCTX, data, frame->nb_samples,
                                     (const uint8_t**)frame->data, frame->nb_samples);
            if (samples > 0)
              output_bytes = samples * inf->ch * inf->sample_fmt_bytes;
            else {
              free(output_data);
              output_data = NULL;
            }
          }
        } else {
          output_bytes = frame->nb_samples * inf->ch * inf->sample_fmt_bytes;
          output_data = frame->data[0];
        }

        if (output_data) {
          audio_buffer_write(tctx.buf, output_data, output_bytes);
          if (output_data != frame->data[0])
            free(output_data);
        }

        av_frame_unref(frame);
      }
    }
    av_packet_unref(packet);

    pthread_mutex_lock(&state->lock);
    int running = state->running;
    pthread_mutex_unlock(&state->lock);
    if (!running) goto done;
  }

  int loop_type;
  pthread_mutex_lock(&state->lock);
  loop_type = state->loop;
  int running = state->running;
  pthread_mutex_unlock(&state->lock);

  if (loop_type == LOOP_TRACK && running) {
    av_seek_frame(fmtCTX, -1, 0, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(decoderCTX);
    total_samples_played = 0;
    goto decode;
  }

done:
  // this for can hear last pieces not skiped
  pthread_mutex_lock(&tctx.buf->lock);
  while (tctx.buf->filled > 0) {
    pthread_mutex_unlock(&tctx.buf->lock);
    usleep(50000); // 50ms
    pthread_mutex_lock(&tctx.buf->lock);
  }
  pthread_cond_broadcast(&tctx.buf->data_ready);
  pthread_mutex_unlock(&tctx.buf->lock);


  pthread_mutex_lock(&state->lock);
  state->running = 0;
  pthread_cond_broadcast(&state->wait_cond);
  pthread_mutex_unlock(&state->lock);

  pthread_mutex_lock(&tctx.buf->lock);
  tctx.buf->stopped = 1;
  pthread_cond_broadcast(&tctx.buf->data_ready);
  pthread_mutex_unlock(&tctx.buf->lock);

  if (swrCTX) swr_free(&swrCTX);
  if (speed_swrCTX) swr_free(&speed_swrCTX);
  av_frame_free(&frame);
  av_packet_free(&packet);

  return NULL;
}
