#ifndef UTILS_H
#define UTILS_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "control.h"
#include "../shared/share_info.h"
#include "../shared/share_utils.h"



void sig_clean(int sig);
void cleanUP();

// void verr(const char *fmt, va_list ap);
// void warn(const char *fmt, ...);
// void die(const char *fmt, ...);

#endif
