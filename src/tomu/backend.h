#ifndef BACKEND_H
#define BACKEND_H

#include "utils.h"
#include "../shared/share_info.h"
#include "../../libs/miniaudio.h"

#if LIBSWRESAMPLE_VERSION_MAJOR <= 3
  #define LEGACY_LIBSWRSAMPLE
#endif

void ma_dataCallback(ma_device *ma_config, void *output, const void *input, ma_uint32 frameCount);
int playback_run(const char *filename, uint loop_mode, uint shuffle_mode);

#endif
