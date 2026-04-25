#ifndef SHARED_UTILS_H
#define SHARED_UTILS_H

#include <stdarg.h>
#include <pthread.h>

// minimal functions
#define WITH_LOCK(mutex) for (int _once = (pthread_mutex_lock(&(mutex)), 1); _once; _once = (pthread_mutex_unlock(&(mutex)), 0)) // pthread mutex

/* socket */
#define write_now_normal_msg(fd, msg) (write(fd, msg, strlen(msg))) // send normal msg socket
#define write_now_struct(fd, structt) write(fd, structt, sizeof(structt)) // send struct socket
#define write_now_enum(fd, enumm) write(fd, &enumm, sizeof(enumm)) // send struct socket

#define read_now_normal_msg(fd) { char buf[128]; int n = read(fd, buf, sizeof(buf)-1); buf[n] = '\0'; printf("%s", buf); } // read normal msg socket

/* for loop */
#define LEN(a) ( sizeof(a) / sizeof(a[0]) ) // get size array
#define for_each_arr(arr) for (int i=0; i<LEN(arr); i++) // for array
#define for_each_num(n) for (int i=0; i<n; i++) // for normal for loop number set

/* time durations */
#define get_hour(a) ((int)a / 3600) // convert time to hour
#define get_min(a) (((int)a % 3600) / 60) // convert time to min
#define get_sec(a) ((int)a % 60) // convert time to sec

void run_command(char *cmd);
char *format(const char *fmt, ...);
void verr(const char *fmt, va_list ap);
int warn(const char *fmt, ...);
void die(const char *fmt, ...);

#endif
