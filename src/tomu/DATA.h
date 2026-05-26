#ifndef DATA_H
#define DATA_H

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <pthread.h>
#include <poll.h>
#include <stdatomic.h>

#include "../../libs/miniaudio.h"
#include "../shared/shared_control.h"
#include "../shared/SHARE_DATA.h"

#if LIBSWRESAMPLE_VERSION_MAJOR <= 3
  #define LEGACY_LIBSWRSAMPLE
#endif

// socket data
typedef struct {
  struct pollfd pfd; // for event if pressed key
  ClientType type;   // type client (because tui & gui will have cover img later)
  int fd;            // fd for client
  int active;        // for make listent the client mode

} CLIENTS_SYSTEM;
// -----------------------------


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
  AVFormatContext *fmtCTX; // Structure audio file
  AVCodecContext *codecCTX; // decoder running
  Audio_Buffer *buf;
  TomuStatus state;
  Audio_Info inf; // information audio
  CLIENTS_SYSTEM client[MAX_CLIENT]; // client handle
  char *queue_list[200];
  pthread_t playback_thread;
  int queue_count;   // how many paths in queue
  int queue_index;   // which one is currently playing
  int playback_active;
  int server_fd;
  int client_fd;
  int discord_rich_presence;

} PlayBackContext;

extern PlayBackContext ctx;

#endif
