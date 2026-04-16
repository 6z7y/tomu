#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "audio_backend.h"
#include "DATA.h"
#include "backend.h"
#include "backend_utils.h"
#include "utils.h"

#include "../../libs/miniaudio.h"

#if LIBSWRESAMPLE_VERSION_MAJOR <= 3
  #define LEGACY_LIBSWRSAMPLE
#endif


// decoder thread
void *run_decoder(void *arg)
{
  AVFormatContext *fmtCTX = ctx.fmtCTX;
  AVCodecContext *codecCTX = ctx.codecCTX;
  Audio_Info *inf = &ctx.inf;
  Audio_State *state = &ctx.state;

  SwrContext *swrCTX = NULL;        // resampler for sample format changes
  SwrContext *speed_swrCTX = NULL;  // Separate resampler for playback speed changes

  // Setup format converter (planar->interleaved if needed)
  if ( av_sample_fmt_is_planar(codecCTX->sample_fmt) ){
    setup_sample_fmt_resampler(inf, &swrCTX);
    
    if (swrCTX) {
      if ( swr_init(swrCTX) < 0 )
        swr_free(&swrCTX);
    }
  }

  AVPacket *packet = av_packet_alloc();
  AVFrame *frame = av_frame_alloc();

  if ( !packet || !frame ) {
    printf("ERROR: Failed to allocate packet/frame\n");
    if (swrCTX) swr_free(&swrCTX);
    if (speed_swrCTX) swr_free(&speed_swrCTX);
    return NULL;
  }

  int64_t total_samples_played = 0;
  int duration_sec = fmtCTX->duration / 1000000;
  ctx.state.duration = duration_sec;
  float last_speed = state->speed;

decode:
  while (av_read_frame(fmtCTX, packet) >= 0) {

    // only process audio packets
    if ( packet->stream_index == inf->audioStream_index ) {

      // send packet to decoder
      if ( avcodec_send_packet(codecCTX, packet) < 0 ) continue;

      // Receive decoded frame
      while (avcodec_receive_frame(codecCTX, frame) >= 0) {
        double current_time = (double)total_samples_played / inf->sample_rate;
        ctx.state.position = (int)current_time;
        total_samples_played += frame->nb_samples;

        pthread_mutex_lock(&state->lock);
          // Handle seek request
          if (state->seek_request) {
            handle_audio_seek(&duration_sec, &total_samples_played);
            av_packet_unref(packet);
            av_frame_unref(frame);
            pthread_mutex_unlock(&state->lock);
            goto decode;
          }
        pthread_mutex_unlock(&state->lock);
        
        // Handle speed change
        if (state->speed != last_speed) {
          last_speed = state->speed;
          
          // Free old speed resampler if exists
          if (speed_swrCTX) {
            swr_free(&speed_swrCTX);
            speed_swrCTX = NULL;
          }
          
          // Create new speed resampler if speed ≠ 1.0
          if (state->speed != 1.0f) {
            setup_speed_resampler(inf, frame, &speed_swrCTX);
          }
        }
        pthread_mutex_unlock(&state->lock);


        // Check pause state
        pthread_mutex_lock(&state->lock);
        while (state->paused)
          pthread_cond_wait(&state->wait_cond, &state->lock);
          
        if (state->running == false) break;
        // progress(state, current_time, duration_sec);
        pthread_mutex_unlock(&state->lock);

        // Process audio based on conversion needs
        uint8_t *output_data = NULL;
        int output_bytes = 0;
        
        if (speed_swrCTX) {
          // Speed conversion (with optional format conversion)
          int out_samples = frame->nb_samples / state->speed;
          
          output_bytes = out_samples * inf->ch * inf->sample_fmt_bytes;
          output_data = malloc(output_bytes);
          
          if (output_data) {
            uint8_t *data_out[1] = {output_data};
            int samples = swr_convert(speed_swrCTX, data_out, out_samples,
                                     (const uint8_t**)frame->data, frame->nb_samples);
            
            if (samples > 0) {
              output_bytes = samples * inf->ch * inf->sample_fmt_bytes;
            } else {
              free(output_data);
              output_data = NULL;
            }
          }
          
        } else if (swrCTX) {
          // Format conversion only (planar->interleaved)
          output_bytes = frame->nb_samples * inf->ch * inf->sample_fmt_bytes;
          output_data = malloc(output_bytes);
          
          if (output_data) {
            uint8_t *data[1] = {output_data};
            int samples = swr_convert(swrCTX, data, frame->nb_samples,
                                     (const uint8_t**)frame->data, frame->nb_samples);
            
            if (samples > 0) {
              output_bytes = samples * inf->ch * inf->sample_fmt_bytes;
            } else {
              free(output_data);
              output_data = NULL;
            }
          }
          
        } else {
          // Direct write (no conversion needed)
          output_bytes = frame->nb_samples * inf->ch * inf->sample_fmt_bytes;
          output_data = frame->data[0];
        }
        
        // Write to buffer
        if (output_data) {
          audio_buffer_write(ctx.buf, output_data, output_bytes);
          
          // Free if we allocated memory (for speed_swrCTX or swrCTX paths)
          if (output_data != frame->data[0]) {
            free(output_data);
          }
        }
        
        av_frame_unref(frame);
      }
    }
    av_packet_unref(packet);


    if (!state->running) break;
  }

  // Handle looping
  if (state->looping && state->running) {
    av_seek_frame(fmtCTX, -1, 0, AVSEEK_FLAG_BACKWARD);
    avcodec_flush_buffers(codecCTX);
    total_samples_played = 0;
    goto decode;
  }

  // Cleanup
  pthread_mutex_lock(&state->lock);
  state->running = 0;
  pthread_cond_broadcast(&state->wait_cond);
  pthread_mutex_unlock(&state->lock);

  // Signal buffer: no more data coming — unblocks ma_dataCallback
  pthread_mutex_lock(&ctx.buf->lock);
  ctx.buf->stopped = 1;
  pthread_cond_broadcast(&ctx.buf->data_ready);
  pthread_mutex_unlock(&ctx.buf->lock);
  
  if (swrCTX) swr_free(&swrCTX);
  if (speed_swrCTX) swr_free(&speed_swrCTX);
  av_frame_free(&frame);
  av_packet_free(&packet);
  return NULL;
}

// this handles playing audio files.
int playback_run(const char *filename, uint loop_mode, uint shuffle_mode)
{
  av_log_set_level(AV_LOG_QUIET); // ignore warning

  // 1. Read file audio
  if (get_audio_info(filename) < 0) return -1;

  get_metadata(); // get metadata
  extract_cover(filename); // extract cover img 

  // initialize a buffer, size = 500ms (Streaming mode)
  int capacity = (ctx.inf.sample_rate) * (ctx.inf.ch) * (ctx.inf.sample_fmt_bytes) * 0.8;
  ctx.buf = audio_buffer_init(capacity);


  // 3. init miniaudio device
  ma_device device;
  ma_device_config ma_config = init_miniaudioConfig(&ctx.inf);

  // 4. initialize the device output
  if (ma_device_init(NULL, &ma_config, &device) != MA_SUCCESS ) goto clean_every;

  // 5. Initialize playback status
  init_playbackstatus(&ctx.state, loop_mode, shuffle_mode);

  // 6. NOW mark as ready! (after all initialization)
  ctx.state.ready = 1;

  // 7. start threads
  pthread_t decoder_thread;
  pthread_create(&decoder_thread, NULL, run_decoder, NULL);
  
  // Start audio playback device
  ma_device_start(&device);

  pthread_join(decoder_thread, NULL);

clean_every:
  ma_device_stop(&device);
  ma_device_uninit(&device);
  audio_buffer_destroy(ctx.buf);
  ctx.buf = NULL;
  pthread_mutex_destroy(&ctx.state.lock);
  pthread_cond_destroy(&ctx.state.wait_cond);
  cleanUP();
  // printf("hi\n");
  // Zero out per-song state so next song starts clean
  // memset(&ctx.inf, 0, sizeof(ctx.inf));
  return 0;
}
