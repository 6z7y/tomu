#ifndef APP_STRUCTS_H
#define APP_STRUCTS_H

#include <curl/curl.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <pthread.h>
#include <stdatomic.h>
#include "dbus/dbus.h"

#include "../../libs/miniaudio.h"

#if LIBSWRESAMPLE_VERSION_MAJOR <= 3
  #define LEGACY_LIBSWRSAMPLE
#endif

// D-Bus data
typedef struct {
 DBusError err; // for error
 DBusConnection *conn; // for connect with dbus
 DBusMessage *msg; // for msg between dbus

} DBus_SYSTEM;
// -----------------------------

typedef enum {
  LOOP_NONE,
  LOOP_TRACK,
  LOOP_PLAYLIST

} LOOP_TYPE;

typedef enum {
  SRC_NONE, // error
  SRC_FILE_DIR,
  SRC_FILE_RAW,
  SRC_URL_PLAYLIST,
  SRC_URL_RAW,

} SRC_TYPE;

typedef struct {
    char title[256];          // song name
    char artist[128];         // who made the song
    char album[128];          // album name
    char album_artist[128];   // album owner
    char genre[128];          // classification like role or key value
    char date[128];           // releases time
    char track[128];          // track number
    char cover_path[512];  // for cover art path
    char url[256];
} Audio_Metadata;

// struct handle Playback
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
  int playback_finsh;
  pthread_mutex_t lock;
  pthread_cond_t wait_cond;

} TomuStatus;

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

typedef struct {
    uint8_t        *data;
    size_t          size;       /* bytes written by curl so far */
    size_t          capacity;
    size_t          read_pos;   /* ffmpeg's current read cursor */
    int             done;       /* set when curl finishes */
    int             error;      /* set on HTTP error */
    pthread_mutex_t lock;
    pthread_cond_t  more_data;
} StreamBuf;

// Ring buffer for PCM data
// In DATA.h, update the AudioRing to store float samples like the test program
typedef struct {
  float *data;            // Changed to float*
  size_t write_pos;
  size_t read_pos;
  size_t count;           /* samples currently held (not bytes) */
  size_t capacity;        /* in samples */
  int channels;
  pthread_mutex_t lock;
  pthread_cond_t has_space;
  pthread_cond_t has_data;
  int stopped;
} AudioRing;

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

typedef struct {
    char **queue_lists;
    int queue_count;
    int queue_index;
    SRC_TYPE src_type;
    int filter_files;
    pthread_t pt;
    pthread_mutex_t lock;
    pthread_cond_t  item_ready;
} LIST_FILES;


typedef struct {
    CURL *curl;              // handle url
    char *stream_url;        // resolved direct audio URL
    char *original_url;      // original user-provided URL
    int is_streaming;        // 1 if this is a streaming URL
    int streaming_active;    // 1 if stream is currently active
    pthread_t stream_thread; // curl download thread
    StreamBuf *stream_buf;   // buffer for streaming data
    int stream_done;         // 1 when streaming is complete
} StreamContext;



// struct for point context used in another functions (needed)
typedef struct PlayBackContext{
  DBus_SYSTEM dbus_s;
  AVFormatContext *fmtCTX; // Structure audio file
  AVCodecContext *codecCTX; // decoder running
  Audio_Buffer *buf;
  TomuStatus state;
  Audio_Info inf; // information audio
  pthread_t playback_thread;
  LIST_FILES list;
  int playback_active;
  StreamContext stream_ctx;
  AudioRing g_ring;

} PlayBackContext;

extern PlayBackContext tctx;

#endif
