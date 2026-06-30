#ifndef UTILS_H
#define UTILS_H


void cleanUP();
void sig_clean(int sig);
void first_init();
unsigned int get_rand();
int is_valid_path(const char *path);
void run_command(char *cmd);
char *format(const char *fmt, ...);
void cleanUP();
void sig_clean(int sig);
int is_url(const char *arg);
void first_init();


#endif
