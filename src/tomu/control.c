#include <sys/poll.h>
#include <termios.h>
#include <unistd.h>
#include <dirent.h>
#include <pthread.h>
#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "control.h"


// static const KeyMap keymap[] = {
// //        KEY                     EXEC
//     { CMD_PLAY_TOGGLE,      playback_toggle         },
//     { CMD_STOP,             playback_stop           },
//     { CMD_NEXT_AUDIO,       playback_next_audio     },
//     { CMD_PREV_AUDIO,       playback_prev_audio     },
//     { CMD_VOL_UP,           volume_increase         },
//     { CMD_VOL_DOWN,         volume_decrease         },
//     { CMD_SPEED_FAST,       playback_speed_increase },
//     { CMD_SPEED_SLOW,       playback_speed_decrease },
//     { CMD_SPEED_DEFAULT,    playback_speed_defualt  },
//     { CMD_SEEK_FORWARD_5S,  seek_forward_sec        },
//     { CMD_SEEK_BACKWARD_5S, seek_backward_sec       },
//     { CMD_SEEK_FORWARD_1M,  seek_forward_min        },
//     { CMD_SEEK_BACKWARD_1M, seek_backward_min       },
//     { CMD_LOOP_TOGGLE,      loop_toggle             },
//     { CMD_SHUFFLE_TOGGLE,   shuffle_toggle          },
// };

// send msg command to all client
// void send_cmd(int sock, Command cmd)
// { write(sock, &cmd, sizeof(Command)); }

// void handle_key(Command cmd, TomuStatus *state)
// {
//   printf("client type %d\n", cmd);
//   for (int i = 0; i < (sizeof(keymap) / sizeof(keymap[0])); i++)
//     if (cmd == keymap[i].key) keymap[i].handler(state);
// }

// control playback (stop/resume/stop)
// functions for playback
// fn toggle pause/resume
void playback_toggle(TomuStatus *state)
{
  WITH_LOCK(state->lock) {
    state->paused = !state->paused;
    pthread_cond_broadcast(&state->wait_cond);
  }
}

// Stops playback and wakes any waiting threads
void playback_stop(TomuStatus *state){
  WITH_LOCK(state->lock) {
    state->skip_to_next = 0;
    state->running = 0;
    pthread_cond_broadcast(&state->wait_cond);
  }
}

// =================================================================

// control audio seek

// Requests a seek forward by 5 seconds
void seek_forward_sec(TomuStatus *state)
{
  WITH_LOCK(state->lock) {
    if (!state->seek_request){
      state->seek_request = 1;
      state->seek_target = +5000000; // +5 sec in microsec
      pthread_cond_broadcast(&state->wait_cond);
    }
  }
}


// Requests a seek forward by 1 min
void seek_forward_min(TomuStatus *state)
{
  WITH_LOCK(state->lock) {
    if (!state->seek_request){
      state->seek_request = 1;
      state->seek_target = +60000000; // +60 sec in microsec
      pthread_cond_broadcast(&state->wait_cond);
    }
  }
}

// Requests a seek backward by 5 seconds
void seek_backward_sec(TomuStatus *state)
{
  WITH_LOCK(state->lock) {
    if (!state->seek_request){
      state->seek_request = 1;
      state->seek_target = -5000000; // -5 sec in microsec
      pthread_cond_broadcast(&state->wait_cond);
    }
  }
}

// Requests a seek backward by 1 min
void seek_backward_min(TomuStatus *state)
{
  WITH_LOCK(state->lock) {
    if (!state->seek_request){
      state->seek_request = 1;
      state->seek_target = -60000000; // -60 sec in microsec
      pthread_cond_broadcast(&state->wait_cond);
    }
  }
}

// =================================================================

// control speed playback
void playback_speed_defualt(TomuStatus *state)
{
  WITH_LOCK(state->lock) {
    state->speed = 1.00f;
  }
}


void playback_speed_increase(TomuStatus *state)
{
  WITH_LOCK(state->lock) {
    state->speed += 0.05f;
    if (state->speed > 2.00f) state->speed = 2.00f;
  }
}

void playback_speed_decrease(TomuStatus *state)
{
  WITH_LOCK(state->lock) {
    state->speed -= 0.05f;
    if (state->speed < 0.25f) state->speed = 0.25f;
  }
}

// control volume playback
inline void volume_increase(TomuStatus *state){
  WITH_LOCK(state->lock) {
    state->volume += 0.02f;
    if (state->volume > 1.26f) state->volume = 1.26f;
  }
}

inline void volume_decrease(TomuStatus *state){
  WITH_LOCK(state->lock) {
    state->volume -= 0.02f;
    if (state->volume < 0.00f) state->volume = 0.00f;
  }
}

// loop toggle
inline void loop_toggle(TomuStatus *state){
  state->looping = !state->looping;
}

// shuffle toggle
void shuffle_toggle(TomuStatus *state) {
  WITH_LOCK(state->lock) {
    state->shuffle = !state->shuffle;
  pthread_cond_broadcast(&state->wait_cond);
  }
}

// prev/next playback
inline void playback_next_audio(TomuStatus *state){
  WITH_LOCK(state->lock) {
    state->skip_to_next = 1;
    state->running = 0;
    pthread_cond_broadcast(&state->wait_cond);
  }
}

inline void playback_prev_audio(TomuStatus *state){
  WITH_LOCK(state->lock) {
    state->skip_to_next = -1;
    state->running = 0;
    pthread_cond_broadcast(&state->wait_cond);
  }
}
