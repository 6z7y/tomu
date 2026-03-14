#ifndef AUDIO_DATA_H
#define AUDIO_DATA_H

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <pthread.h>
#include <stdint.h>
#include "../../libs/miniaudio.h"

#if LIBSWRESAMPLE_VERSION_MAJOR <= 3
  #define LEGACY_LIBSWRSAMPLE
#endif


typedef struct {
    char *title;          // song name
    char *artist;         // who made the song
    char *album;          // album name
    char *album_artist;   // album owner
    char *genre;          // classification
    char *date;           // releases time
    char *track;          // track number
} Audio_Metadata;


// struct handle Playback
typedef struct {
  int running;
  int paused;
  int duration;
  int position;
  int skip_to_next;
  float volume;
  float speed;
  uint looping;
  uint shuffle;
  int seek_request;
  int64_t seek_target;
  int ready;
  pthread_mutex_t lock;
  pthread_cond_t wait_cond;
} Audio_State;

typedef struct {
  uint8_t *pcm_data;
  int capacity;
  int write_pos;
  int read_pos;  
  int filled;
  int stopped;
  pthread_mutex_t lock;
  pthread_cond_t data_ready;
  pthread_cond_t space_free;
} Audio_Buffer;

// struct for base information of audio file (codec)
typedef struct {
  int audioStream_index;
  AVStream *audioStream;
  int ch;
  #ifdef LEGACY_LIBSWRSAMPLE
    int64_t ch_layout;
  #else
    AVChannelLayout ch_layout;
  #endif
  int sample_rate;
  enum AVSampleFormat sample_fmt;
  int sample_fmt_bytes;
  ma_format ma_fmt;
} Audio_Info;


// struct for point context used in another functions (needed)
typedef struct PlayBackContext{
  int server_fd;
  int client_fd;
  Audio_Metadata metadata;
  Audio_State state;
  Audio_Buffer *buf;
  Audio_Info inf;
  int playback_active;
  char *queue_list[200];
  int queue_count;   // how many paths in queue
  int queue_index;   // which one is currently playing
  pthread_t playback_thread;
  AVFormatContext *fmtCTX;
  AVCodecContext *codecCTX;
} PlayBackContext;

extern PlayBackContext ctx;

#endif
