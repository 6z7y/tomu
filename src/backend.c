#include "backend.h"
#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavutil/frame.h>
#include <libswresample/swresample.h>
#include <libavutil/avutil.h>
#include <libswresample/version.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>
#include <string.h>
#include <unistd.h>

#define MINIAUDIO_IMPLEMENTATION
#include "libs/miniaudio.h"

#if LIBSWRESAMPLE_VERSION_MAJOR <= 3 
  #define LEGACY_LIBSWRSAMPLE
#endif

typedef struct {
  uint8_t *PCM_data;
  int capacity;
  int write_postion;
  int read_postion;
  int size;
  pthread_mutex_t lock;
  pthread_cond_t data_avaliable;
  pthread_cond_t space_avaliable;

} AudioBuffer;

typedef struct {
  int audioStream;
  int ch;
  int ch_layout;
  int sample_rate;
  enum AVSampleFormat sample_fmt;
  int sample_fmt_bytes;
  ma_format ma_fmt;

} AudioInfo;

typedef struct {
  AudioBuffer *buf;
  AudioInfo *inf;
  AVFormatContext *fmtCTX;
  AVCodecContext *codecCTX;

} StreamContext;

void cleanUP(AVFormatContext *fmtCTX, AVCodecContext *codecCTX){
  if (fmtCTX ) avformat_close_input(&fmtCTX);
  if (codecCTX ) avcodec_free_context(&codecCTX);
}

int shinu_now(const char *msg, AVFormatContext *fmtCTX, AVCodecContext *codecCTX){
  fprintf(stderr, "F: %s\n", msg);

  cleanUP(fmtCTX, codecCTX);

  return 1;
}

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

AudioBuffer *audio_buffer_crate(int capacity){
  AudioBuffer *buf = malloc(sizeof(AudioBuffer));
  buf->PCM_data = malloc(capacity);
  buf->capacity = capacity;
  buf->write_postion = 0;
  buf->read_postion = 0;
  buf->size = 0;
  pthread_mutex_init(&buf->lock, NULL);
  pthread_cond_init(&buf->data_avaliable, NULL);
  pthread_cond_init(&buf->space_avaliable, NULL);
  return buf;
}

void audio_buffer_destroy(AudioBuffer *buf){
  if (buf ){
    free(buf->PCM_data);
    pthread_mutex_destroy(&buf->lock);
    pthread_cond_destroy(&buf->data_avaliable);
    pthread_cond_destroy(&buf->space_avaliable);
    free (buf);
  }
}

void audio_buffer_write(AudioBuffer *buf, uint8_t *data, int bytes_write){
  pthread_mutex_lock(&buf->lock);
  while(buf->size + bytes_write > buf->capacity){
    pthread_cond_wait(&buf->space_avaliable, &buf->lock);
  }

  int chunk = buf->capacity - buf->write_postion;
  if (chunk > bytes_write ){
    chunk = bytes_write;
  }

  memcpy(buf->PCM_data + buf->write_postion, data, chunk);
  memcpy(buf->PCM_data, data + chunk, bytes_write - chunk);

  buf->write_postion = (buf->write_postion + bytes_write) % buf->capacity;
  buf->size += bytes_write;

  pthread_cond_signal(&buf->data_avaliable);

  pthread_mutex_unlock(&buf->lock);
}

void audio_buffer_read(AudioBuffer *buf, uint8_t *output, int bytes_read){
  pthread_mutex_lock(&buf->lock);
  while(buf->size == 0){
    pthread_cond_wait(&buf->data_avaliable, &buf->lock);
  }

  if (bytes_read > buf->size){
    bytes_read = buf->size;
  }

  int chunk = buf->capacity - buf->read_postion;
  if (chunk > bytes_read ){
    chunk = bytes_read;
  }

  memcpy(output, buf->PCM_data + buf->read_postion, chunk);
  memcpy(output + chunk, buf->PCM_data, bytes_read - chunk);

  buf->read_postion = (buf->read_postion + bytes_read) % buf->capacity;
  buf->size -= bytes_read;

  pthread_cond_signal(&buf->space_avaliable);
  pthread_mutex_unlock(&buf->lock);
}

void *decoder_place(void *arg){
  StreamContext *streamCTX = (StreamContext*)arg;
  AVFormatContext *fmtCTX = streamCTX->fmtCTX;
  AVCodecContext * codecCTX = streamCTX->codecCTX;
  SwrContext *swrCTX = NULL;
  AudioInfo *inf = streamCTX->inf;

  #ifdef LEGACY_LIBSWRSAMPLE
    swrCTX = swr_alloc_set_opts(swrCTX,
      inf->ch_layout, inf->sample_fmt, inf->sample_rate,
      inf->ch_layout, codecCTX->sample_fmt, inf->sample_rate,
      0, NULL
    );
  #else
    swr_alloc_set_opts2(&swrCTX,
       inf->ch_layout, inf->sample_fmt, inf->sample_rate,
       inf->ch_layout, codecCTX->sample_fmt, inf->sample_rate,
    );
  #endif

  if (!swrCTX || swr_init(swrCTX) < 0 ){
    swr_free(&swrCTX);
  }

  AVPacket *packet = av_packet_alloc();
  AVFrame *frame = av_frame_alloc();

  if (!packet || !frame ){
    printf("something happend when init packet or frame!\n");
    if (swrCTX ) swr_free(&swrCTX);
    return NULL;
  }

  // int64_t total_samples_played = 0;
  // double duration_sec = fmtCTX->duration / 1000000.0; // seconds

  while (av_read_frame(fmtCTX, packet) >= 0){
    if (packet->stream_index == inf->audioStream){
      if (avcodec_send_packet(codecCTX, packet) < 0 ){
        continue;
      }

      while (avcodec_receive_frame(codecCTX, frame) >= 0){
        if (swrCTX ){
          uint8_t *data_conv = malloc(frame->nb_samples * inf->ch * inf->sample_fmt_bytes);
          uint8_t *data[1] = {data_conv};

          int samples = swr_convert(swrCTX,
            data, frame->nb_samples,
            (const uint8_t**)frame->data, frame->nb_samples
          );

          if (samples > 0 ){
            int bytes = samples * inf->ch * inf->sample_fmt_bytes;
            audio_buffer_write(streamCTX->buf, data[0], bytes);
          }
        } else{
          int bytes = frame->nb_samples * inf->ch * inf->sample_fmt_bytes;
          audio_buffer_write(streamCTX->buf, *frame->data, bytes);
        }

        // double current_time_sec = (double)total_samples_played / inf->sample_rate;
        //
        // printf("\r%02d:%02d / %02d:%02d", 
        //        (int)current_time_sec / 60, (int)current_time_sec % 60,
        //        (int)duration_sec / 60, (int)duration_sec % 60);
        // fflush(stdout);
        // total_samples_played += frame->nb_samples;

        av_frame_unref(frame);
      }
    }
    av_packet_unref(packet);
  }

  if (swrCTX ) swr_free(&swrCTX);
  av_frame_free(&frame);
  av_packet_free(&packet);
  return NULL;
}
  
void ma_dataCallback(ma_device *ma_config, void *output, const void *input, ma_uint32 frameCount){
  StreamContext *streamCTX = (StreamContext*)ma_config->pUserData;
  AudioInfo *inf = streamCTX->inf;

  int bytes = frameCount * inf->ch * inf->sample_fmt_bytes;
  audio_buffer_read(streamCTX->buf, output, bytes);
}

void stream_play(StreamContext *streamCTX){
  pthread_t decoder_thread;
  AudioInfo *inf = streamCTX->inf;

  int capacity = (inf->sample_rate) * (inf->ch) * (inf->sample_fmt_bytes) * 0.5;
  streamCTX->buf = audio_buffer_crate(capacity);

  ma_device device;
  ma_device_config ma_config = ma_device_config_init(ma_device_type_playback);

  ma_config.playback.channels = inf->ch;
  ma_config.playback.format = inf->ma_fmt;
  ma_config.sampleRate = inf->sample_rate;
  ma_config.dataCallback = ma_dataCallback;
  ma_config.pUserData = streamCTX;

  if (ma_device_init(NULL, &ma_config, &device) != MA_SUCCESS ){
    audio_buffer_destroy(streamCTX->buf);
    return;
  }

  pthread_create(&decoder_thread, NULL, decoder_place, streamCTX);
  ma_device_start(&device);
  pthread_join(decoder_thread, NULL);
  usleep(30000);
  audio_buffer_destroy(streamCTX->buf);
  ma_device_uninit(&device);
  return;
}

int scan_now(const char *filename){
  AVFormatContext *fmtCTX = NULL;
  AVCodecContext *codecCTX = NULL;
  AudioInfo inf = {0};
  StreamContext streamCTX = {0};

  if (avformat_open_input(&fmtCTX, filename, NULL, NULL) < 0 ){
    return shinu_now("can't open container file", fmtCTX, NULL);
  }

  if (avformat_find_stream_info(fmtCTX, NULL) < 0 ){
    return shinu_now("can't find any streams!", fmtCTX, NULL);
  }

  int audioStream = -1;;
  for (int i = 0; i < fmtCTX->nb_streams; i++){
    AVStream *stream = fmtCTX->streams[i];
    if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO ){
      audioStream = i;
      break;
    }
  }

  if (audioStream == -1 ){
    return shinu_now("can't get audioStream!", fmtCTX, NULL);
  }

  const AVCodecParameters *codecPAR = fmtCTX->streams[audioStream]->codecpar;
  const AVCodec *codecID = avcodec_find_decoder(codecPAR->codec_id);

  codecCTX = avcodec_alloc_context3(codecID);

  if (!codecCTX ){
    return shinu_now("can't allocate codec!", fmtCTX, NULL);
  }

  avcodec_parameters_to_context(codecCTX, codecPAR);

  if (avcodec_open2(codecCTX, codecID, NULL) < 0){
    return shinu_now("can't init decoder!", fmtCTX, codecCTX);
  }

  enum AVSampleFormat input_sample_fmt = codecCTX->sample_fmt;
  enum AVSampleFormat output_sample_fmt = input_sample_fmt;
  
  if (av_sample_fmt_is_planar(input_sample_fmt)){
    output_sample_fmt = get_interleaved(input_sample_fmt);
  }

  inf.audioStream = audioStream;

  #ifdef LEGACY_LIBSWRSAMPLE
    inf.ch = codecCTX->channels;
  #else
    inf.ch = codecCTX->ch_layout.nb_channels;
  #endif

  #ifdef LEGACY_LIBSWRSAMPLE
    inf.ch_layout = codecCTX->channel_layout;
  #else
    AVChannelLayout ch_layout;
      
    av_channel_layout_default(&ch_layout, &inf.ch);
    inf.ch_layout = ch_layout; 
  #endif

  inf.sample_rate = codecCTX->sample_rate;
  inf.sample_fmt = output_sample_fmt;
  inf.sample_fmt_bytes = av_get_bytes_per_sample(inf.sample_fmt);
  inf.ma_fmt = get_ma_format(output_sample_fmt);

  streamCTX.inf = &inf;
  streamCTX.fmtCTX = fmtCTX;
  streamCTX.codecCTX = codecCTX;

  printf("Playing: %s\n", filename);
  printf("%dHz, %dch, %s\n", inf.sample_rate, inf.ch, av_get_sample_fmt_name(inf.sample_fmt));

  stream_play(&streamCTX);

  cleanUP(fmtCTX, codecCTX);
  return 0;
}
