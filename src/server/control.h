#ifndef CONTROL_H
#define CONTROL_H

#include <poll.h>

#include "audio_data.h"

typedef enum {
    CLIENT_CLI,
    CLIENT_TUI,
    CLIENT_GUI,
} ClientType;

typedef struct {
  int fd;
  int active;
  struct pollfd pfd;
  ClientType type;
} Client_connection;

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


void handle_key(Command cmd, Audio_State *state);

void playback_toggle(Audio_State *state);
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

void shuffle_toggle(Audio_State *state);
void loop_toggle(Audio_State *state);

void playback_next_audio(Audio_State *state);
void playback_prev_audio(Audio_State *state);

#endif
