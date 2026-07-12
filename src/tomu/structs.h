#ifndef STRUCTS_H
#define STRUCTS_H

#include <curl/curl.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <pthread.h>
#include <stdatomic.h>
#include <dbus/dbus.h>

#include "../../libs/miniaudio.h"

#if LIBSWRESAMPLE_VERSION_MAJOR <= 3
  #define LEGACY_LIBSWRSAMPLE
#endif

typedef struct {
  DBusError err;
  DBusConnection *conn;
  DBusMessage *msg;
  pthread_t thread;
} DBus_SYSTEM;

typedef enum {
  LOOP_NONE,
  LOOP_TRACK,
  LOOP_PLAYLIST
} LOOP_TYPE;

typedef enum {
  SRC_NONE,
  SRC_FILE_DIR,
  SRC_FILE_RAW,
  SRC_URL_PLAYLIST,
  SRC_URL_RAW,
} SRC_TYPE;

typedef struct {
  char title[256];
  char artist[128];
  char album[128];
  char album_artist[128];
  char composer[128];
  char disc[128];
  char genre[128];
  char date[128];
  char track[128];
  char cover_path[512];
  char url[256];
} Audio_Metadata;

typedef struct {
  Audio_Metadata metadata;
  int running;
  int paused;
  int duration;
  int position;
  int skip_to_next;
  float volume;
  float speed;
  LOOP_TYPE loop;
  int shuffle;
  int seek_request;
  int64_t seek_target;
  pthread_mutex_t lock;
  pthread_cond_t wait_cond;
} PlaybackStatus;

typedef struct {
  ma_device device;
  uint8_t *pcm_data;
  size_t capacity;
  size_t write_pos;
  size_t read_pos;
  int filled;
  int stopped;
  pthread_mutex_t lock;
  pthread_cond_t data_ready;
  pthread_cond_t space_free;
} Audio_Buffer;

typedef struct {
  uint8_t        *data;
  size_t          size;
  size_t          capacity;
  size_t          read_pos;
  int             done;
  int             error;
  pthread_mutex_t lock;
  pthread_cond_t  more_data;
} StreamBuf;

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

typedef struct {
  char **queue_lists;
  int queue_count;
  int queue_index;

  int *queue_history;
  int queue_history_count;

  SRC_TYPE src_type;
  int filter_files;
  pthread_t pt;
  pthread_mutex_t pt_lock;
  pthread_cond_t  pt_signal;
} LIST_FILES;

typedef struct {
  char *stream_url;
  int is_streaming;
  AVIOContext *avio;   // so streaming_stop() can free it safely
  pthread_t stream_thread;
  StreamBuf *stream_buf;
} StreamContext;

typedef struct PlayBackContext {
  DBus_SYSTEM dbus_s;
  AVFormatContext *fmtCTX;
  AVCodecContext *decoderCTX;
  Audio_Buffer *buf;
  PlaybackStatus state;
  Audio_Info inf;
  LIST_FILES list;
  StreamContext stream_ctx;
} PlayBackContext;

extern PlayBackContext tctx;

#endif
