#ifndef SHARED_CONTROL_H
#define SHARED_CONTROL_H

#include <stdint.h>
#include <stdatomic.h>
#include <pthread.h>

typedef enum {
    KY_ESC = 27,

    KY_SPACE = ' ',
    KY_PLUS = '+',
    KY_MINUS = '-',
    KY_LBRACKET = '[',
    KY_RBRACKET = ']',
    KY_BACKSPACE = 127,

    // custom keys (not ASCII)
    KY_UP = 1000,
    KY_DOWN,
    KY_RIGHT,
    KY_LEFT,

    KY_ENTER = 1100,
} KeyCode;

typedef enum {
  CMD_PLAY_TOGGLE,
  CMD_STOP,
  CMD_NEXT_AUDIO,
  CMD_PREV_AUDIO,
  CMD_VOL_UP,
  CMD_VOL_DOWN,
  CMD_SPEED_SLOW,
  CMD_SPEED_FAST,
  CMD_SPEED_DEFAULT,
  CMD_SEEK_FORWARD_5S,
  CMD_SEEK_FORWARD_1M,
  CMD_SEEK_BACKWARD_5S,
  CMD_SEEK_BACKWARD_1M,
  CMD_LOOP_TOGGLE,
  CMD_SHUFFLE_TOGGLE,

  CMD_PATH,
  CMD_FIRST_INFOS,
  CMD_STATUS,
  CMD_END

} Command;

typedef struct {
    char title[128];          // song name
    char artist[128];         // who made the song
    char album[128];          // album name
    char album_artist[128];   // album owner
    char genre[128];          // classification like role or key value
    char date[128];           // releases time
    char track[128];          // track number

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

#endif
