#ifndef UTILS_H
#define UTILS_H

void cleanUP();
void signal_handle(int sig);
void first_init();
unsigned int get_rand();
int run_command(char *cmd);
char *format(const char *fmt, ...);

#endif
