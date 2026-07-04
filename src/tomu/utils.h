#ifndef UTILS_H
#define UTILS_H

void cleanUP();
void signal_handle(int sig);
void first_init();
unsigned int get_rand();
void run_command(char *cmd);
char *format(const char *fmt, ...);
void cleanUP();
void sig_clean(int sig);
void first_init();

#endif
