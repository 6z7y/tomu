#ifndef SHARED_UTILS1_H
#define SHARED_UTILS1_H

#include <stdarg.h>
#include <pthread.h>
#include <unistd.h>

#include "shared_control.h"

// minimal functions
#define WITH_LOCK(mutex) for (int _once = (pthread_mutex_lock(&(mutex)), 1); _once; _once = (pthread_mutex_unlock(&(mutex)), 0)) // pthread mutex

/* socket */
#define write_now_normal_msg(fd, msg) (write(fd, msg, strlen(msg))) // send normal msg to server
#define write_now_struct(fd, structt, sizee) write(fd, structt, sizee) // send struct to server
#define write_now_enum(fd, type, enumm) type cmd = (enumm); write((fd), &cmd, sizeof(cmd));  // send enum cmd to server

#define read_now_normal_msg(fd) { char buf[128]; int n = read(fd, buf, sizeof(buf)-1); buf[n] = '\0'; printf("%s", buf); } // read normal msg socket

/* for loop */
#define LEN(a) ( sizeof(a) / sizeof(a[0]) ) // get size array
#define for_each_arr(arr) for (int i=0; i<LEN(arr); i++) // for array
#define for_each_num(n) for (int i=0; i<n; i++) // for normal for loop number set


// Utils
#define F_ALSE 0
#define T_RUE 1

// colors
#define RED "\e[0;31m"
#define CYN "\e[0;36m"
#define WHT "\e[0;37m"
#define MAG "\e[0;35m"
#define GRN "\e[0;32m"

// sleeping
#define sleep_ms(n) usleep(1000*n)

unsigned int get_rand();
int is_valid_path(const char *path);
void run_command(char *cmd);
char *format(const char *fmt, ...);
void verr(const char *fmt, va_list ap);
int warn(const char *fmt, ...);
void die(const char *fmt, ...);

#endif
