#ifndef CONTROL_H
#define CONTROL_H

#include "../share-clients.h"

void handle_control(int sock, const char *key);
void send_cmd(int sock, Command cmd);

void send_path(int sock, const char *path);
void path_change(int sock, const char *key);

#endif
