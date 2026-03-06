#ifndef AUDIO_DATA_H
#define AUDIO_DATA_H

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libswresample/swresample.h>
#include "../../libs/miniaudio.h"

#if LIBSWRESAMPLE_VERSION_MAJOR <= 3
  #define LEGACY_LIBSWRSAMPLE
#endif

typedef struct {
  int   duration;
  int   position;
  int   paused;
  float volume;
  float speed;
  int   shuffle;
  int   loop;
} TomuStatus;

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
  int position;  // add this
  int skip_to_next;
  float volume;
  float speed;
  uint looping;
  uint shuffle;
  int seek_request; // Flag: 1 = seek needed
  int64_t seek_target; // Where seek to (in microseconds)
  int ready;  // ← ADD THIS: indicates playback is ready
  pthread_mutex_t lock;
  pthread_cond_t wait_cond;

} Audio_State;

typedef struct {
  uint8_t *pcm_data;           // Audio data storage
  int capacity;                // Total size in bytes
  int write_pos;               // Where to write next
  int read_pos;                // Where to read next  
  int filled;                  // How many bytes are stored now
  int stopped;                 // 1 = decoder done, no more data coming
  pthread_mutex_t lock;        // Protect from multiple threads
  pthread_cond_t data_ready;   // Signal when data available
  pthread_cond_t space_free;   // Signal when space available

} Audio_Buffer;

// struct for base information of audio file (codec)
typedef struct {
  int audioStream_index;
  AVStream *audioStream;
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

} Audio_Info;

// struct for data of the files in dir
typedef struct {
  int totalFiles;
  int currentFile;
  // this for cleaning (needed)
  char** files;
} Dir_File;

// struct for point context used in another functions (needed)
typedef struct PlayBackContext{
  int server_fd;
  int client_fd;
  Audio_Metadata metadata;
  Audio_State state;
  Audio_Buffer *buf;
  Audio_Info inf;
  Dir_File dir;
  int playback_active;
  pthread_t playback_thread;
  AVFormatContext *fmtCTX;
  AVCodecContext *codecCTX;

} PlayBackContext;

extern PlayBackContext ctx;

#endif
