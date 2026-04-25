#ifndef CONTROL_H
#define CONTROL_H

#include "../shared/shared_control.h"
#include "../shared/SHARE_DATA.h"

void handle_control(int *server_fd, const char *key);
void termios_mode(int mode);

#endif
