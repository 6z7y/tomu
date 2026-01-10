#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/log.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <libswresample/version.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "backend.h"
#include "control.h"
#include "socket.h"
#include "utils.h"

#define MINIAUDIO_IMPLEMENTATION
#include "libs/miniaudio.h"

#if LIBSWRESAMPLE_VERSION_MAJOR <= 3
  #define LEGACY_LIBSWRSAMPLE
#endif

// function take from planar_value to get interleaved_value
enum AVSampleFormat get_interleaved(enum AVSampleFormat value){
  switch (value){
    case AV_SAMPLE_FMT_DBLP: return AV_SAMPLE_FMT_DBL;
    case AV_SAMPLE_FMT_FLTP: return AV_SAMPLE_FMT_FLT;
    case AV_SAMPLE_FMT_S64P: return AV_SAMPLE_FMT_S64;
    case AV_SAMPLE_FMT_S32P: return AV_SAMPLE_FMT_S32;
    case AV_SAMPLE_FMT_S16P: return AV_SAMPLE_FMT_S16;
    case AV_SAMPLE_FMT_U8P: return AV_SAMPLE_FMT_U8;
    default: return AV_SAMPLE_FMT_S16; // fallback
  }
}

// function take from interleaved_value get mini audio format
ma_format get_ma_format(enum AVSampleFormat value){
  switch (value){
    case AV_SAMPLE_FMT_DBL: return ma_format_f32;
    case AV_SAMPLE_FMT_FLT: return ma_format_f32;
    case AV_SAMPLE_FMT_S64: return ma_format_s32;
    case AV_SAMPLE_FMT_S32: return ma_format_s32;
    case AV_SAMPLE_FMT_S16: return ma_format_s16;
    case AV_SAMPLE_FMT_U8: return ma_format_u8;
    default: return ma_format_s16; // fallback
  }
}

AudioBuffer *audio_buffer_init(int capacity){
  AudioBuffer *buf = malloc(sizeof(AudioBuffer));

  buf->pcm_data = malloc(capacity);
  buf->capacity = capacity;
  buf->write_pos = 0;     // Start writing at beginning
  buf->read_pos = 0;      // Start reading from beginning
  buf->filled = 0;        // Buffer starts empty

  pthread_mutex_init(&buf->lock, NULL);
  pthread_cond_init(&buf->data_ready, NULL);
  pthread_cond_init(&buf->space_free, NULL);
  return buf;
}

void audio_buffer_destroy(AudioBuffer *buf){
  if (buf ){
    free(buf->pcm_data);
    pthread_mutex_destroy(&buf->lock);
    pthread_cond_destroy(&buf->data_ready);
    pthread_cond_destroy(&buf->space_free);
    free (buf);
  }
}

// WRITE AUDIO DATA TO BUFFER
void audio_buffer_write(AudioBuffer *buf, uint8_t *audio_data, int data_size) {
  // Step 1: Lock so only one thread can write at a time
  pthread_mutex_lock(&buf->lock);
  
  // Step 2: Wait if buffer doesn't have enough space
  // filled + new_data must fit in total capacity
  while (buf->filled + data_size > buf->capacity) {
    pthread_cond_wait(&buf->space_free, &buf->lock);
  }
  
  // Step 3: How much space until end of buffer?
  int space_until_end = buf->capacity - buf->write_pos;
  
  // Step 4: Check if all data fits before end
  if (data_size <= space_until_end) {
    // Case 1: All data fits without wrapping
    memcpy(buf->pcm_data + buf->write_pos, audio_data, data_size);
  } else {
    // Case 2: Need to wrap around
    // Part A: Fill until end of buffer
    memcpy(buf->pcm_data + buf->write_pos, audio_data, space_until_end);
    
    // Part B: Wrap to beginning for remaining bytes
    int remaining = data_size - space_until_end;
    memcpy(buf->pcm_data, audio_data + space_until_end, remaining);
  }
  
  // Step 5: Update write position (wrap around if needed)
  buf->write_pos = (buf->write_pos + data_size) % buf->capacity;
  
  // Step 6: Update how much data is in buffer
  buf->filled += data_size;
  
  // Step 7: Signal that data is now available for reading
  pthread_cond_signal(&buf->data_ready);
  
  // Step 8: Unlock for other threads
  pthread_mutex_unlock(&buf->lock);
}

// READ AUDIO DATA FROM BUFFER
void audio_buffer_read(AudioBuffer *buf, uint8_t *output, int bytes_needed) {
  // Step 1: Lock so only one thread can read at a time
  pthread_mutex_lock(&buf->lock);
  
  // Step 2: Wait if buffer is empty
  while (buf->filled == 0) {
    pthread_cond_wait(&buf->data_ready, &buf->lock);
  }
  
  // Step 3: Don't try to read more than what's available
  int bytes_to_read = bytes_needed;
  if (bytes_to_read > buf->filled) {
    bytes_to_read = buf->filled;
  }
  
  // Step 4: How much data until end of buffer?
  int data_until_end = buf->capacity - buf->read_pos;
  
  // Step 5: Check if all data we need is before end
  if (bytes_to_read <= data_until_end) {
    // Case 1: All data available without wrapping
    memcpy(output, buf->pcm_data + buf->read_pos, bytes_to_read);
  } else {
    // Case 2: Need to wrap around
    // Part A: Read until end of buffer
    memcpy(output, buf->pcm_data + buf->read_pos, data_until_end);
    
    // Part B: Wrap to beginning for remaining bytes
    int remaining = bytes_to_read - data_until_end;
    memcpy(output + data_until_end, buf->pcm_data, remaining);
  }
  
  // Step 6: Update read position (wrap around if needed)
  buf->read_pos = (buf->read_pos + bytes_to_read) % buf->capacity;
  
  // Step 7: Update how much data is left in buffer
  buf->filled -= bytes_to_read;
  
  // Step 8: Signal that space is now free for writing
  pthread_cond_signal(&buf->space_free);
  
  // Step 9: Unlock for other threads
  pthread_mutex_unlock(&buf->lock);
}

void *run_decoder(void *arg){
  StreamContext *streamCTX = (StreamContext*)arg;
  AVFormatContext *fmtCTX = streamCTX->fmtCTX;
  AVCodecContext * codecCTX = streamCTX->codecCTX;
  SwrContext *swrCTX = NULL;
  AudioInfo *inf = streamCTX->inf;
  PlayBackState *state = streamCTX->state;

  #ifdef LEGACY_LIBSWRSAMPLE
    swrCTX = swr_alloc_set_opts(swrCTX,
      inf->ch_layout, inf->sample_fmt, inf->sample_rate, // output
      inf->ch_layout, codecCTX->sample_fmt, inf->sample_rate, // input
      0, NULL
    );
  #else
    swr_alloc_set_opts2(&swrCTX,
      &inf->ch_layout, inf->sample_fmt, inf->sample_rate, // output
      &inf->ch_layout, codecCTX->sample_fmt, inf->sample_rate, // input
      0, NULL
    );
  #endif

  if (!swrCTX || swr_init(swrCTX) < 0 )
    swr_free(&swrCTX);

  AVPacket *packet = av_packet_alloc();
  AVFrame *frame = av_frame_alloc();

  if (!packet || !frame ){
    printf("something happend when init packet or frame!\n");
    if (swrCTX ) swr_free(&swrCTX);
    return NULL;
  }

  int64_t total_samples_played = 0;
  int duration_time = fmtCTX->duration / 1000000.0;


  // first we read the data from container format (.mp3, .opus, .flac, ...etc)
  while (av_read_frame(fmtCTX, packet) >= 0){

    // we need only audio stream
    if (packet->stream_index == inf->audioStream){

      // sent packet to frame decoder
      if (avcodec_send_packet(codecCTX, packet) < 0 ){
        continue;
      }

      // frame take it as PCM samples (for used to miniaudio send to speaker for a played)
      while (avcodec_receive_frame(codecCTX, frame) >= 0){
        // init duration
        double current_time = (double)total_samples_played / inf->sample_rate;
        printf("\r  %d:%02d:%02d / %d:%02d:%02d | vol: %.0f%%.",
          get_hour(current_time), get_min(current_time), get_sec(current_time), 
          get_hour(duration_time), get_min(duration_time), get_sec(duration_time),
          state->volume * 100
        );
        fflush(stdout);
        total_samples_played += frame->nb_samples;

        // run this if plnar: convert from planar to interleaved
        if (swrCTX ){
          uint8_t *data_conv = malloc(frame->nb_samples * inf->ch * inf->sample_fmt_bytes);

          if (!data_conv){
            fprintf(stderr, "Error: Out of memory for audio convertion\n");
            av_frame_unref(frame);
            continue;
          }

          uint8_t *data[1] = {data_conv};

          // start convert the samples
          int samples = swr_convert(swrCTX,
            data, frame->nb_samples, // output
            (const uint8_t**)frame->data, frame->nb_samples // input
          );

          if (samples > 0 ){
            // get how much bytes write from this (PCM samples)
            int bytes = samples * inf->ch * inf->sample_fmt_bytes;
            // write in buffer
            audio_buffer_write(streamCTX->buf, data[0], bytes);
          }
          free(data_conv);

          // run this if: already interleaved
        } else{
          // get how much bytes write from this (PCM samples)
          int bytes = frame->nb_samples * inf->ch * inf->sample_fmt_bytes;
          // write in buffer
          audio_buffer_write(streamCTX->buf, frame->data[0], bytes);
        }
        av_frame_unref(frame);
      }
    }
    av_packet_unref(packet);


    pthread_mutex_lock(&state->lock);
    while (state->paused){
      pthread_cond_wait(&state->wait_cond, &state->lock);
    }
    pthread_mutex_unlock(&state->lock);

    if (!state->running) break;
  }

  printf("\n");

  if (swrCTX ) swr_free(&swrCTX);
  av_frame_free(&frame);
  av_packet_free(&packet);
  return NULL;
}
  
// miniaudio will use this function everytime for reading PCM samples
void ma_dataCallback(ma_device *ma_config, void *output, const void *input, ma_uint32 frameCount){
  StreamContext *streamCTX = (StreamContext*)ma_config->pUserData;
  AudioInfo *inf = streamCTX->inf;
  PlayBackState *state = streamCTX->state;
  
  // Get volume safely
  float current_volume;
  pthread_mutex_lock(&state->lock);
  current_volume = state->volume;
  pthread_mutex_unlock(&state->lock);
  
  // Check paused state
  pthread_mutex_lock(&state->lock);

  while (state->paused) {
    pthread_cond_wait(&state->wait_cond, &state->lock);
  }

  pthread_mutex_unlock(&state->lock);
  
  // Read audio data
  int bytes = frameCount * inf->ch * inf->sample_fmt_bytes;
  audio_buffer_read(streamCTX->buf, output, bytes);
  
  // Apply volume (already have safe copy)
  if (current_volume != 1.00f) {
    ma_apply_volume_factor_pcm_frames(output, frameCount, inf->ma_fmt, inf->ch, current_volume);
  }
}

// init mini audio config before used
ma_device_config init_miniaudioConfig(AudioInfo *inf, StreamContext *streamCTX){
  ma_device_config ma_config = ma_device_config_init(ma_device_type_playback);

  ma_config.playback.channels = inf->ch;
  ma_config.playback.format = inf->ma_fmt;
  ma_config.sampleRate = inf->sample_rate;
  ma_config.dataCallback = ma_dataCallback;
  ma_config.pUserData = streamCTX;

  return ma_config;
}

// this function is responsible of running workers and starting playback
void stream_audio(StreamContext *streamCTX){
  AudioInfo *inf = streamCTX->inf;
  PlayBackState *state = streamCTX->state;

  pthread_t control_thread;
  pthread_t sock_thread;
  pthread_t decoder_thread;

  // init a buffer size = 500ms
  int capacity = (inf->sample_rate) * (inf->ch) * (inf->sample_fmt_bytes) * 0.5;
  streamCTX->buf = audio_buffer_init(capacity);

  // init miniaudio engine (for sending PCM samples to speaker)
  ma_device device;
  ma_device_config ma_config = init_miniaudioConfig(inf, streamCTX);

  // initialize the device output
  if (ma_device_init(NULL, &ma_config, &device) != MA_SUCCESS ){
    audio_buffer_destroy(streamCTX->buf);
    pthread_mutex_destroy(&state->lock);
    pthread_cond_destroy(&state->wait_cond);
    return;
  }

  // Sets terminal mode to raw and hides cursor
  struct termios orig_mode = init_terminal();

  // exec 3 worker threads
  pthread_create(&control_thread, NULL, handle_input, streamCTX->state);
  pthread_create(&sock_thread, NULL, run_socket, streamCTX->state);
  pthread_create(&decoder_thread, NULL, run_decoder, streamCTX);
  
  // start device and wait till there's not more samples to play
  ma_device_start(&device);
  pthread_join(decoder_thread, NULL);

  // Restores term mode to the original one
  deinit_terminal(&orig_mode);


  // audio is end now must off all thing (I'll keep this it's funny)
  // cleanup after playback finishes
  pthread_cancel(control_thread);
  pthread_cancel(sock_thread);

  ma_device_stop(&device);
  ma_device_uninit(&device);
  audio_buffer_destroy(streamCTX->buf);
  pthread_mutex_destroy(&state->lock);
  pthread_cond_destroy(&state->wait_cond);
  return;
}

// This function for get information from a file
// first trying extract information & save values with used in stream_audio()
int playback_run(const char *filename){
  AVFormatContext *fmtCTX = NULL;
  AVCodecContext *codecCTX = NULL;

  av_log_set_level(AV_LOG_QUIET);

  if (avformat_open_input(&fmtCTX, filename, NULL, NULL) < 0 )
    die("file: file type is not supported");

  if (avformat_find_stream_info(fmtCTX, NULL) < 0 )
    die("ffmpeg: cannot find any streams");

  // he we get stream audio index from container
  int audioStream = -1;
  for (int i = 0; i < fmtCTX->nb_streams; i++){
    AVStream *stream = fmtCTX->streams[i];
    if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO ){
      audioStream = i;
      break;
    }
  }

  if (audioStream == -1 )
	die("file: file type is not supported");

  // here we get the information about audio stream is codecParameters
  const AVCodecParameters *codecPAR = fmtCTX->streams[audioStream]->codecpar;

  // codecID mean the key of decoder needed for decode as PCM samples (later)
  const AVCodec *codecID = avcodec_find_decoder(codecPAR->codec_id);

  // init empty decoder
  codecCTX = avcodec_alloc_context3(codecID);

  if (!codecCTX )
    die("ffmpeg: cannot allocate codec!");

  // Copy audio specification to decoder
  avcodec_parameters_to_context(codecCTX, codecPAR);

  // initialize decoder with actual codec
  if (avcodec_open2(codecCTX, codecID, NULL) < 0)
    die("ffmpeg: cannot init decoder!");

  // Audio samples can be stored in two formats: PLANAR or INTERLEAVED
  // 
  // PLANAR (separate channels):           INTERLEAVED (mixed channels):
  //   Channel 0: [L L L L L L]              [L R L R L R L R L R]
  //   Channel 1: [R R R R R R]
  // 
  // Speakers need INTERLEAVED format! We must convert PLANAR to INTERLEAVED.
  // here not mean is converted now, not yet (later)
  enum AVSampleFormat input_sample_fmt = codecCTX->sample_fmt;
  enum AVSampleFormat output_sample_fmt = input_sample_fmt;
  
  if (av_sample_fmt_is_planar(input_sample_fmt)){
    output_sample_fmt = get_interleaved(input_sample_fmt);
  }

  // setup PlaybackState
  PlayBackState state = {
    .running = 1,
    .paused = 0,
    .volume = 1.00f
  };
  pthread_mutex_init(&state.lock, NULL);
  pthread_cond_init(&state.wait_cond, NULL);

  // Save Base information to AudioInfo struct
  AudioInfo inf = {
    #ifdef LEGACY_LIBSWRSAMPLE
      .ch = codecCTX->channels,
      .ch_layout = codecCTX->channel_layout,
    #else
      .ch = codecCTX->ch_layout.nb_channels,
      .ch_layout = codecCTX->ch_layout,
    #endif

    .audioStream = audioStream,
    .sample_rate = codecCTX->sample_rate,
    .sample_fmt = output_sample_fmt,
    .sample_fmt_bytes = av_get_bytes_per_sample(inf.sample_fmt),
    .ma_fmt = get_ma_format(output_sample_fmt),
  };

  // Pointer Contexts for used in an another functions
  StreamContext streamCTX = {
    .inf = &inf,
    .fmtCTX = fmtCTX,
    .codecCTX = codecCTX,
    .state = &state,
  };

  printf("Playing: %s\n",  filename);
  printf("%.2dHz, %dch, %s\n", inf.sample_rate, inf.ch, av_get_sample_fmt_name(inf.sample_fmt));

  // play audio
  stream_audio(&streamCTX);

  cleanUP(fmtCTX, codecCTX);
  return 0;
}
