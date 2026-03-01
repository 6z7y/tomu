#ifndef CONTROL_H
#define CONTROL_H

#include "backend.h"
#define SOCKET_PATH "/tmp/tomu-sock"
#define MAX_CLIENT 6

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
  CMD_SHUFFLE_TOGGEL,
  CMD_PATH,
} Command;

typedef struct {
    int   duration;
    int   position;
    int   paused;
    float volume;
    float speed;
    int   shuffle;
    int   loop;
    int   playing;
    char  current_file[256];
} TomuStatus;

void handle_key(Command cmd, Audio_State *state);

void playback_toggle(Audio_State *state);
void playback_pause(Audio_State *state);
void playback_resume(Audio_State *state);
void playback_stop(Audio_State *state);


void seek_forward_sec(Audio_State *state);
void seek_forward_min(Audio_State *state);
void seek_backward_sec(Audio_State *state);
void seek_backward_min(Audio_State *state);

void playback_speed_defualt(Audio_State *state);
void playback_speed_increase(Audio_State *state);
void playback_speed_decrease(Audio_State *state);

void volume_increase(Audio_State *state);
void volume_decrease(Audio_State *state);

void do_shuffle();
void stopAndShuffle(Audio_State* state);
void shuffle_toggle(Audio_State *state);
void shuffle_true(Audio_State *state);
void shuffle_false(Audio_State *state);

void loop_toggle(Audio_State *state);
void loop_true(Audio_State *state);
void loop_false(Audio_State *state);

void playback_next_audio(Audio_State *state);
void playback_prev_audio(Audio_State *state);

#endif
