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

#if LIBSWRESAMPLE_VERSION_MAJOR <= 3
  #define LEGACY_LIBSWRSAMPLE
#endif

// minimal functions
#define WITH_LOCK(mutex) for (int _once = (pthread_mutex_lock(&(mutex)), 1); _once; _once = (pthread_mutex_unlock(&(mutex)), 0)) // pthread mutex

#define read_now_normal_msg(fd) { char buf[128]; int n = read(fd, buf, sizeof(buf)-1); buf[n] = '\0'; printf("%s", buf); } // read normal msg socket

/* for loop */
#define LEN(a) ( sizeof(a) / sizeof(a[0]) ) // get size array
#define for_each_arr(arr) for (int i=0; i<LEN(arr); i++) // for array
#define for_each_num(n) for (int i=0; i<n; i++) // for normal for loop number set

// server info
#define TOMU_NAME "tomu"
#define TOMU_VER "1.3.4"

// sleeping
#define sleep_ms(n) usleep(1000*n)

/* time durations */
#define get_hour(a) ((int)a / 3600) // convert time to hour
#define get_min(a) (((int)a % 3600) / 60) // convert time to min
#define get_sec(a) ((int)a % 60) // convert time to sec

#define BUS_NAME    "org.mpris.MediaPlayer2.tomu"
#define OBJ_PATH    "/org/mpris/MediaPlayer2"
#define IFACE_ROOT  "org.mpris.MediaPlayer2"
#define IFACE_PLAYER "org.mpris.MediaPlayer2.Player"
#define IFACE_PROPS "org.freedesktop.DBus.Properties"


// Utils
#define F_ALSE 0
#define T_RUE 1

// colors
#define RED "\e[0;31m"
#define CYN "\e[0;36m"
#define WHT "\e[0;37m"
#define MAG "\e[0;35m"
#define GRN "\e[0;32m"

// D-Bus data
typedef struct {
 DBusError err; // for error
 DBusConnection *conn; // for connect with dbus
 DBusMessage *msg; // for msg between dbus

} DBus_SYSTEM;
// -----------------------------


typedef struct {
    char title[128];          // song name
    char artist[128];         // who made the song
    char album[128];          // album name
    char album_artist[128];   // album owner
    char genre[128];          // classification like role or key value
    char date[128];           // releases time
    char track[128];          // track number
    char cover_path[512];  // ADD THIS for cover art path
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
  atomic_int looping;
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
    int filter_files;
    pthread_t pt;
    pthread_mutex_t lock;
    pthread_cond_t  item_ready;
} LIST_FILES;


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
  StreamContext stream_ctx;
  AudioRing g_ring;

} PlayBackContext;

extern PlayBackContext tctx;

#endif
