#ifndef SHARED_CONTROL_H
#define SHARED_CONTROL_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

typedef struct {
  int   duration;
  int   position;
  int   paused;
  float volume;
  float speed;
  int   shuffle;
  int   loop;

} TomuStatus;

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
  CMD_PATH,
  CMD_QUIT_SERVER_FULL,

} Command;


// void verr(const char *fmt, va_list ap)
// {
// 	vfprintf(stderr, fmt, ap);
// 	if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
// 		fputc(' ', stderr);
// 		perror(NULL);
// 	} else {
// 		fputc('\n', stderr);
// 	}
// }
//
// void warn(const char *fmt, ...)
// {
// 	va_list ap;
// 	va_start(ap, fmt);
// 	verr(fmt, ap);
//   va_end(ap);
// }
//
// void die(const char *fmt, ...)
// {
// 	va_list ap;
// 	va_start(ap, fmt);
//   verr(fmt, ap);
// 	va_end(ap);
// 	exit(-1);
// }

#endif
