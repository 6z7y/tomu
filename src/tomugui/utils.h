#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

void sig_clean(int sig);
void clean_with_bye(int *server_fd);
void socket_mode(int *server_fd, int mode);

#endif

