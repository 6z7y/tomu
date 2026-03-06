#ifndef SOCKET_UTILS_H
#define SOCKET_UTILS_H

#include "backend_utils.h"

void socket_mode(int mode, int *server_fd);
int add_client_into_poll(struct pollfd *fds, Client_connection *client);
void broadcast_status(Client_connection *client);
void accept_new_client(int *server_fd, int *client_fd, Client_connection *client);
void handle_client_events(int i, int *index, struct pollfd *fds, Client_connection *client);
void client_die(int i, Client_connection *client);
void start_playback(const char *path);
void send_cmd(int sock, Command cmd);

#endif
