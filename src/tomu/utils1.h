#ifndef UTILS1_H
#define UTILS1_H

#include <stdarg.h>

void cleanUP();
void sig_clean(int sig);
void first_init();

// int is_url(const char *arg);

unsigned int get_rand();
int is_valid_path(const char *path);
void run_command(char *cmd);
char *format(const char *fmt, ...);
void verr(const char *fmt, va_list ap);
int warn(const char *fmt, ...);
void die(const char *fmt, ...);


#endif
