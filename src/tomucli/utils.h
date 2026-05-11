#ifndef UTILS_H
#define UTILS_H

#include <stdio.h>

void sig_clean(int sig);
void client_socket_mode(int *server_fd, int ON);
void init_context();

#endif
