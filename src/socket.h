#ifndef SOCKET_H
#define SOCKET_H

#define SOCKET_PATH "/tmp/tomu-sock"

void *run_socket(void *arg);
void cleanup_socket();

#endif
