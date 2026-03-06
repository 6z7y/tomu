#ifndef SHARE_CLIENTS_H
#define SHARE_CLIENTS_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

typedef enum {
    CLIENT_CLI,
    CLIENT_TUI,
    CLIENT_GUI,
} ClientType;

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
  CMD_PATH

} Command;

typedef struct {
  char *key;
  Command cmd;
} KeyMap;

static inline int get_sec(double value){
  return (int)value % 60;
}

static inline int get_min(double value){
  return ((int)value % 3600) / 60;
}

static inline int get_hour(double value){
  return (int)value / 3600;
}


// void verr(const char *fmt, va_list ap);
// void warn(const char *fmt, ...);
// void die(const char *fmt, ...);

#endif
