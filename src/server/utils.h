#ifndef UTILS_H
#define UTILS_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "control.h"
#include "../shared/share_info.h"

void sig_clean();
void cleanUP();

void socket_mode(int mode, int *server_fd);

// int is_audio(const char *file);
// void path_handle(const char *path, uint loop_mode, uint shuffle_mode, uint skip_fmt_mode);

int args_handle(char **argv);

void verr(const char *fmt, va_list ap);
void warn(const char *fmt, ...);
void die(const char *fmt, ...);

#endif
