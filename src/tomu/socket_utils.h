#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include "backend_utils.h"
#include <pthread.h>

int add_client_into_poll(struct pollfd *fds, CLIENTS_SYSTEM *client);
void broadcast_status(CLIENTS_SYSTEM *client);
void accept_new_client(int *server_fd, int *client_fd, CLIENTS_SYSTEM *client);
void start_playback(char *path);
void handle_client_events(int i, struct pollfd *fds, CLIENTS_SYSTEM *client);
void *playback_thread_func(void *arg);
void server_socket_mode(int *server_fd, int ON);

#endif
