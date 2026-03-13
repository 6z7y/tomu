#ifndef BACKEND_H
#define BACKEND_H

#include "../../shared/shared_control.h"

void print_header(const char *filename, int sample_rate, int channels, const char *fmt_name);
void progress(TomuStatus *status, double current_time, int duration_time);

#endif
