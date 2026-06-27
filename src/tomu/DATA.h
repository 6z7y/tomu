#ifndef DATA_H
#define DATA_H

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include <pthread.h>
#include <stdatomic.h>
#include "dbus/dbus.h"

#include "../../libs/miniaudio.h"
#include "../shared/shared_control.h"

#if LIBSWRESAMPLE_VERSION_MAJOR <= 3
  #define LEGACY_LIBSWRSAMPLE
#endif

#define BUS_NAME    "org.mpris.MediaPlayer2.tomu"
#define OBJ_PATH    "/org/mpris/MediaPlayer2"
#define IFACE_ROOT  "org.mpris.MediaPlayer2"
#define IFACE_PLAYER "org.mpris.MediaPlayer2.Player"
#define IFACE_PROPS "org.freedesktop.DBus.Properties"

// D-Bus data
typedef struct {
 DBusConnection *conn;
 DBusMessage *msg;

} DBus_SYSTEM;
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

typedef struct {
  char **queue_lists; // list
  int queue_count;   // how many paths in queue
  int queue_index;   // which one is currently playing
  int filter_files;

} LIST_FILES;

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

typedef struct {
    char *stream_url;        // resolved direct audio URL
    char *original_url;      // original user-provided URL
    int is_streaming;        // 1 if this is a streaming URL
    int streaming_active;    // 1 if stream is currently active
    pthread_t stream_thread; // curl download thread
    StreamBuf *stream_buf;   // buffer for streaming data
    int stream_done;         // 1 when streaming is complete
} StreamContext;


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
  StreamContext stream_ctx; // ADD THIS
  AudioRing g_ring;


} PlayBackContext;

extern PlayBackContext tctx;

#endif
