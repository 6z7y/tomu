#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/samplefmt.h>
#include <libswresample/swresample.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>

#define MINIAUDIO_IMPLEMENTATION
#include "libs/miniaudio.h"

#if LIBSWRESAMPLE_VERSION_MAJOR <= 3
  #define LEGACY_LIBRESAMPLE
#endif

#define PROG_VER "0.0.2"

typedef struct {
  int audioStream;
  int channels;
  int channel_layout;
  int sample_rate;
  enum AVSampleFormat sample_fmt;
  int sample_fmt_bytes;
  ma_format ma_fmt;
  int ma_fmt_bytes;

} AudioInfo;

typedef struct {
  uint8_t *data;
  int capacity;
  int read_postion;
  int write_postion;
  int size;
  pthread_mutex_t lock;
  pthread_cond_t data_avaliable;
  pthread_cond_t space_avaliable;

} AudioBuffer;


typedef struct {
  AudioBuffer *buf;
  AudioInfo *inf;
  AVFormatContext *fmtCTX;
  AVCodecContext *codecCTX;
  pthread_t decoder;

} StreamContext;

void help(){
  printf(
    "Usage: tomu [COMMAND] [PATH]\n"
    " Commands:\n"

    "   loop            : loop same sound\n"
    "   shuffle-loop    : select random file audio and loop\n"
    "   version         : show version of program\n"
    "   help            : show help message\n"
  );
}

void clean_free(AVFormatContext *fmtCTX, AVCodecContext *codecCTX){
  if (codecCTX ) avcodec_free_context(&codecCTX);
  if (fmtCTX ) avformat_close_input(&fmtCTX);
}

int shinu_now(char *msg, const char *filename, AVFormatContext *fmtCTX, AVCodecContext *codecCTX){
  if (filename )
    fprintf(stderr, "F: %s '%s'", msg, filename);
  else
    fprintf(stderr, "F: %s", msg);

  clean_free(fmtCTX, codecCTX);

  exit(1);
}

void audio_buffer_destroy(AudioBuffer *buf){
  if (buf ){
    free(buf->data);
    pthread_mutex_destroy(&buf->lock);
    pthread_cond_destroy(&buf->data_avaliable);
    pthread_cond_destroy(&buf->space_avaliable);
    free(buf);
  }
}

AudioBuffer *audio_buffer_create(int capacity){
  AudioBuffer *buf = malloc(sizeof(AudioBuffer));
  buf->data = malloc(capacity);
  buf->capacity = capacity;
  buf->write_postion = 0;
  buf->read_postion = 0;
  buf->size = 0;
  pthread_mutex_init(&buf->lock, NULL);
  pthread_cond_init(&buf->data_avaliable, NULL);
  pthread_cond_init(&buf->space_avaliable, NULL);
  return buf;
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

  memcpy(buf->data + buf->write_postion, data, chunk);
  memcpy(buf->data, data + chunk, bytes_write - chunk);

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

  memcpy(output, buf->data + buf->read_postion, chunk);
  memcpy(output + chunk, buf->data, bytes_read - chunk);

  buf->read_postion = (buf->read_postion + bytes_read) % buf->capacity;
  buf->size -= bytes_read;

  pthread_cond_signal(&buf->space_avaliable);
  pthread_mutex_unlock(&buf->lock);
}

void *decoder_place(void *arg){
  StreamContext *streamCTX = (StreamContext*)arg;
  AVFormatContext *fmtCTX = streamCTX->fmtCTX;
  AVCodecContext *codecCTX = streamCTX->codecCTX;
  SwrContext *swrCTX = NULL;
  AudioInfo *inf = streamCTX->inf;

  #ifdef LEGACY_LIBRESAMPLE
    swrCTX = swr_alloc_set_opts(swrCTX,
      inf->channel_layout, inf->sample_fmt, inf->sample_rate,
      codecCTX->channel_layout, codecCTX->sample_fmt, codecCTX->sample_rate,
      0, NULL
    );
  #else
    swrCTX = swr_alloc_set_opts2(&swrCTX,
      &codecCTX->ch_layout, inf->sample_fmt, inf->sample_rate, 
      &codecCTX->ch_layout, codecCTX->sample_fmt, codecCTX->sample_rate, 
      0, NULL
    );
  #endif

  if (!swrCTX || swr_init(swrCTX) < 0 ){
    swr_free(&swrCTX);
  }

  AVPacket *packet = av_packet_alloc();
  AVFrame *frame = av_frame_alloc();

  if (!packet || !frame ){
    printf("something happend when alloc packet or frame!");
  }

  while (av_read_frame(fmtCTX, packet) >= 0){
    if (packet->stream_index == inf->audioStream ){
      if (avcodec_send_packet(codecCTX, packet) < 0 ){
        printf("can't read this packet!");
        continue;
      }
      while (avcodec_receive_frame(codecCTX, frame) == 0){
        if (swrCTX ){
          uint8_t *converted_data = malloc(frame->nb_samples * inf->channels * inf->sample_fmt_bytes);
          uint8_t *out_data[1] = {converted_data};

          int samples = swr_convert(swrCTX, out_data, frame->nb_samples, (const uint8_t**)frame->data, frame->nb_samples);

          if (samples > 0 ){
            int bytes_write = samples * inf->channels * inf->sample_fmt_bytes;

            audio_buffer_write(streamCTX->buf, *out_data, bytes_write);
            free(converted_data);
          }
        } else{
          int bytes_write = frame->nb_samples * inf->channels * inf->sample_fmt_bytes;

          audio_buffer_write(streamCTX->buf, *frame->data, bytes_write);
        }
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

void ma_callback(ma_device *device, void *output, const void *input, ma_uint32 frameCount){
  StreamContext *streamCTX = (StreamContext*)device->pUserData;

  int bytes_read = frameCount * streamCTX->inf->channels * av_get_bytes_per_sample(streamCTX->inf->sample_fmt);
  audio_buffer_read(streamCTX->buf, output, bytes_read);
}

void stream_play(StreamContext *streamCTX){
  AudioInfo *inf = streamCTX->inf;
  int capacity = (inf->sample_rate) * (inf->channels) * (inf->sample_fmt_bytes) * 1;
  streamCTX->buf = audio_buffer_create(capacity);

  ma_device_config miniaudio_config = ma_device_config_init(ma_device_type_playback);
  ma_device device;

  miniaudio_config.playback.channels = inf->channels;
  miniaudio_config.sampleRate = inf->sample_rate;
  miniaudio_config.playback.format = inf->ma_fmt;
  miniaudio_config.dataCallback = ma_callback;
  miniaudio_config.pUserData = streamCTX;

  if (ma_device_init(NULL, &miniaudio_config, &device) != MA_SUCCESS ){
    audio_buffer_destroy(streamCTX->buf);
    return;
  }

  pthread_create(&streamCTX->decoder, NULL, decoder_place, streamCTX);
  ma_device_start(&device);
  pthread_join(streamCTX->decoder, NULL);
  sleep(1);
  ma_device_uninit(&device);
  audio_buffer_destroy(streamCTX->buf);
  return;
}

enum AVSampleFormat get_interleaved(enum AVSampleFormat value){
  switch(value) {
    case AV_SAMPLE_FMT_DBLP: return AV_SAMPLE_FMT_DBL;
    case AV_SAMPLE_FMT_FLTP: return AV_SAMPLE_FMT_FLT;
    case AV_SAMPLE_FMT_S64P: return AV_SAMPLE_FMT_S64;
    case AV_SAMPLE_FMT_S32P: return AV_SAMPLE_FMT_S32;
    case AV_SAMPLE_FMT_S16P: return AV_SAMPLE_FMT_S16;
    case AV_SAMPLE_FMT_U8P: return  AV_SAMPLE_FMT_U8;
    default: return AV_SAMPLE_FMT_S16; // fallback
  }
}

ma_format get_ma_fmt(enum AVSampleFormat value){
  switch(value){
    case AV_SAMPLE_FMT_DBLP: case AV_SAMPLE_FMT_DBL: return ma_format_f32;
    case AV_SAMPLE_FMT_FLTP: case AV_SAMPLE_FMT_FLT: return ma_format_f32;
    case AV_SAMPLE_FMT_S64P: case AV_SAMPLE_FMT_S64: return ma_format_s32;
    case AV_SAMPLE_FMT_S32P: case AV_SAMPLE_FMT_S32: return ma_format_s32;
    case AV_SAMPLE_FMT_S16P: case AV_SAMPLE_FMT_S16: return ma_format_s16;
    case AV_SAMPLE_FMT_U8P: case AV_SAMPLE_FMT_U8: return ma_format_u8;
    default: return ma_format_s16; // fallback
  }
}

int start_check(const char *filename){
  AVFormatContext *fmtCTX = NULL;
  AVCodecContext *codecCTX = NULL;
  AudioInfo inf = {0};
  StreamContext streamCTX = {0};

  if (avformat_open_input(&fmtCTX, filename, NULL, NULL) < 0 ){
    shinu_now("can't open file", filename, NULL, NULL);
  }

  if (avformat_find_stream_info(fmtCTX, NULL) < 0){
    shinu_now("not found any Streams!", filename, fmtCTX, NULL);
  }

  int audioStream = -1;

  for (int i = 0; i < fmtCTX->nb_streams; i++){
    AVStream *stream = fmtCTX->streams[i];
    if (stream->codecpar->codec_type == AVMEDIA_TYPE_AUDIO ){
      audioStream = i;
      break;
    }
  }

  if (audioStream == -1 ){
    shinu_now("i can't get a audioStream!", NULL, fmtCTX, NULL);
  }

  const AVCodecParameters *codecPAR = fmtCTX->streams[audioStream]->codecpar;
  const AVCodec *codecID = avcodec_find_decoder(codecPAR->codec_id);

  codecCTX = avcodec_alloc_context3(codecID);

  if (!codecCTX ){
    shinu_now("can't allocate codecContext!", NULL, fmtCTX, NULL);
  }

  avcodec_parameters_to_context(codecCTX, codecPAR);

  if (avcodec_open2(codecCTX, codecID, NULL) < 0 ){
    shinu_now("something happend when make decoder ready!", NULL, fmtCTX, codecCTX);
  }

  enum AVSampleFormat input_sample_fmt = codecCTX->sample_fmt;
  enum AVSampleFormat output_sample_fmt = input_sample_fmt;

  if (av_sample_fmt_is_planar(input_sample_fmt) == 1 ){
    output_sample_fmt = get_interleaved(input_sample_fmt);
  }

  inf.audioStream = audioStream;

  #ifdef LEGACY_LIBRESAMPLE
    inf.channels = codecCTX->channels;
  #else
    inf.channels = codecCTX->ch_layout.nb_channels;
  #endif

  inf.sample_rate = codecCTX->sample_rate;
  inf.sample_fmt = output_sample_fmt;
  inf.sample_fmt_bytes = av_get_bytes_per_sample(inf.sample_fmt);
  inf.ma_fmt = get_ma_fmt(inf.sample_fmt);

  streamCTX.fmtCTX = fmtCTX;
  streamCTX.codecCTX = codecCTX;
  streamCTX.inf = &inf;

  printf("Playing file: %s\n", filename);
  printf("%dHz, ch %d, format %s\n", inf.sample_rate, inf.channels, av_get_sample_fmt_name(inf.sample_fmt));

  if (inf.channels > 2 ){
    printf("downmix channels from %d to 2 (for compatlibilty)", inf.channels);
    inf.channels = 2;
  }

  if (inf.channels == 1){
    inf.channel_layout = AV_CH_LAYOUT_MONO;
  } else{
    inf.channel_layout = AV_CH_LAYOUT_STEREO;
  }

  stream_play(&streamCTX);

  clean_free(fmtCTX, codecCTX);
  return 0;
}


void shuffle_now(const char *path){
  srand(time(NULL));
  struct dirent *entry;

  DIR *dir = opendir(path);
  if (!dir){
    perror("dir");
  }

  int count = 0;
  while ((entry = readdir(dir)) != NULL){
    if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0) continue;


    count++;
  }

  int index_rand = rand() % count;
  rewinddir(dir);

  for (int i = 0; i <= index_rand;){
    entry = readdir(dir);
    if (strcmp(".", entry->d_name) == 0 || strcmp("..", entry->d_name) == 0) continue;


    if (i == index_rand){
      char filename[259];
      snprintf(filename, sizeof(filename), "%s/%s", path, entry->d_name);
      start_check(filename);
    }
    i++;
  }
  printf("something happend\n");
}


int main(int argc, char *argv[]){
  if (argc < 2 ){
    printf("Usage: %s [FILE]\n", argv[0]);
    return 0;
  }

  const char *command = argv[1];
  const char *filename = argv[argc - 1];

  if (strcmp("loop", command) == 0 ){
    while (1){
      start_check(filename);
    }
  } else if (strcmp("shuffle-loop", command) == 0){
    while (1){
      shuffle_now(filename);
    }

  } else if (strcmp("version", command) == 0 ){
    printf("%s\n", PROG_VER);

  } else if (strcmp("help", command) == 0){
    help();

  } else{
    start_check(filename);
  }

  return 0;
}
