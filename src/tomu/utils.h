#ifndef UTILS_H
#define UTILS_H

#include "structs.h"

void cleanUP(PlayBackContext *ctx);
void signal_handle(int sig);
unsigned int get_rand();
int run_command(char *cmd);
char *format(const char *fmt, ...);
void init_threads(PlayBackContext *ctx);
void *read_cmd(void *arg);
void write_inf(PlayBackContext *ctx);

#endif
