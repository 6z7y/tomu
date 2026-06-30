#ifndef BACKEND_H
#define BACKEND_H

#include "../../libs/miniaudio.h"

void ma_dataCallback(ma_device *ma_config, void *output, const void *input, ma_uint32 frameCount);
int playback_run(const char *filename, int loop_mode, int shuffle_mode);

#endif
