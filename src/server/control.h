#ifndef CONTROL_H
#define CONTROL_H

#include <poll.h>

#include "audio_data.h"
#include "../shared/shared_control.h"

typedef enum {
  CLIENT_CLI,
  CLIENT_TUI,
  CLIENT_GUI,

} ClientType;

typedef struct {
  int fd;            // fd for client
  int active;        // for make listent the client mode
  struct pollfd pfd; // for event if pressed key
  ClientType type;   // type client (because tui & gui will have cover img later)
} Client_connection;

void send_cmd(int sock, Command cmd);

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
