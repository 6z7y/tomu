#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

void socket_mode(int mode, int *server_fd);
void clean_with_bye(int socket, int mode);
void termios_mode(int mode);
void help();

// void verr(const char *fmt, va_list ap);
// void warn(const char *fmt, ...);
// void die(const char *fmt, ...);

#endif
