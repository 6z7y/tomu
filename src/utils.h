#ifndef UTILS_H
#define UTILS_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#define false 0
#define true 1
extern uint KeepPlayingDirectory;

void help();
void cleanUP();
void path_handle(const char *path, uint loop_mode, uint shuffle_mode);

void verr(const char *fmt, va_list ap);
void warn(const char *fmt, ...);
void die(const char *fmt, ...);

#endif
