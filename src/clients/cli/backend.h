#ifndef BACKEND_H
#define BACKEND_H

#include "../share_clients.h"

typedef struct {
    int   duration;
    int   position;
    int   paused;
    float volume;
    float speed;
    int   shuffle;
    int   loop;
} TomuStatus;

void print_header(const char *filename, int sample_rate, int channels, const char *fmt_name);
void progress(TomuStatus *status, double current_time, int duration_time);


#endif
