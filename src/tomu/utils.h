#ifndef UTILS_H
#define UTILS_H

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>

#include "control.h"
#include "../shared/share_info.h"
#include "../shared/share_utils.h"

// minimal functions
#define write_socket_normal_msg(socket, msg) write(socket, msg, strlen(msg)) // write to client normal msg

#define write_socket_struct(socket, msg) write(socket, msg, sizeof(msg)) // write to client struct ver

#define LEN(a) ( sizeof(a) / sizeof(a[0]) ) // get size array

#define for_each_arr(arr) for (int i=0; i<LEN(arr); i++) // for array

#define for_each_num(n) for (int i=0; i<n; i++) // for normal for loop number set


void sig_clean(int sig);
void cleanUP();

// void verr(const char *fmt, va_list ap);
// void warn(const char *fmt, ...);
// void die(const char *fmt, ...);

#endif
