#ifndef UTILS_H
#define UTILS_H

void socket_mode(int mode, int *server_fd);
void clean_with_bye(int socket, int mode);
void termios_mode(int mode);
void help();

#endif
