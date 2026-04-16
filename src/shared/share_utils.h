#ifndef SHARED_UTILS_H
#define SHARED_UTILS_H

#include <stdarg.h>

// minimal functions
#define write_socket_normal_msg(socket, msg) write(socket, msg, strlen(msg)) // write to client normal msg

#define write_socket_struct(socket, msg) write(socket, msg, sizeof(msg)) // write to client struct ver

#define LEN(a) ( sizeof(a) / sizeof(a[0]) ) // get size array

#define for_each_arr(arr) for (int i=0; i<LEN(arr); i++) // for array

#define for_each_num(n) for (int i=0; i<n; i++) // for normal for loop number set

void verr(const char *fmt, va_list ap);
int warn(const char *fmt, ...);
void die(const char *fmt, ...);

#endif
