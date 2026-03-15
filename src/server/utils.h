#ifndef UTILS_H
#define UTILS_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "control.h"
#include "../shared/share_info.h"

void sig_clean(int sig);
void cleanUP();

void socket_mode(int mode, int *server_fd);

int args_handle(const char *option);

void verr(const char *fmt, va_list ap);
void warn(const char *fmt, ...);
void die(const char *fmt, ...);

#endif
