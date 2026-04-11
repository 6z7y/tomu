#ifndef SHARED_CONTROL_H
#define SHARED_CONTROL_H

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
  // CMD_QUIT_SERVER_FULL,

} Command;

typedef struct {
  int   duration;
  int   position;
  int   paused;
  float volume;
  float speed;
  int   shuffle;
  int   loop;

} TomuStatus;

#endif
