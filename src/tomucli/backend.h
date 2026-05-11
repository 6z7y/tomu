#ifndef BACKEND_H
#define BACKEND_H

#include "CLIENT_DATA.h"
#include "../shared/shared_control.h"

int update_status(int fd, int *was_playing, TomuStatus *status, PlaybackQueue *queue);
void progress(TomuStatus *status, double current_time, int duration_time);

#endif
