#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include "backend_utils.h"
#include <pthread.h>

int add_client_into_poll(struct pollfd *fds, Client_connection *client);
void broadcast_status(Client_connection *client);
void accept_new_client(int *server_fd, int *client_fd, Client_connection *client);
void client_checker_event(int nfds, struct pollfd *fds, Client_connection *client);
void start_playback(char *path);
void send_cmd(int sock, Command cmd);
void *playback_thread_func(void *arg);
void socket_mode(int mode, int *server_fd);

#endif
