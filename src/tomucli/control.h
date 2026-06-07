#ifndef CONTROL_H
#define CONTROL_H

#include "../shared/shared_control.h"
#include "../shared/SHARE_DATA.h"

void send_path(int server_fd, const char *path);
void handle_control(int server_fd);
void termios_mode(int ON);

#endif
