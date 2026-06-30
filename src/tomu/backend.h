#ifndef BACKEND_H
#define BACKEND_H

#include "utils1.h"
#include "utils2.h"
#include "../../libs/miniaudio.h"
#include <sys/types.h>

void ma_dataCallback(ma_device *ma_config, void *output, const void *input, ma_uint32 frameCount);
int playback_run(const char *filename, uint loop_mode, uint shuffle_mode);

#endif
