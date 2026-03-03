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

void progress(TomuStatus *status, double current_time, int duration_time);


#endif
