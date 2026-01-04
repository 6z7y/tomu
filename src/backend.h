#ifndef BACKEND_H
#define BACKEND_H

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include "libs/miniaudio.h"

#if LIBSWRESAMPLE_VERSION_MAJOR <= 3
  #define LEGACY_LIBSWRSAMPLE
#endif

typedef struct {
  int running;
  int paused;
  pthread_mutex_t lock;
  pthread_cond_t waitKudasai;

} PlayBackState;

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
  #ifdef LEGACY_LIBSWRSAMPLE
    int ch_layout;
  #else
    AVChannelLayout ch_layout;
  #endif
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
  PlayBackState *state;

} StreamContext;

int scan_now(const char *filename);

#endif
