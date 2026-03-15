#ifndef CONTROL_H
#define CONTROL_H

#include "../../shared/shared_control.h"
#include "../share_data.h"

inline void send_cmd(int *server_fd, Command cmd);
void handle_control(int *server_fd, const char *key);

void send_path(int sock, const char *path);

#endif
