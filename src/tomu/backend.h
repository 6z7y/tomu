#ifndef BACKEND_H
#define BACKEND_H

#include "utils.h"
#include "../shared/SHARE_DATA.h"
#include "../../libs/miniaudio.h"

void ma_dataCallback(ma_device *ma_config, void *output, const void *input, ma_uint32 frameCount);
int playback_run(const char *filename, uint loop_mode, uint shuffle_mode);

#endif
