#ifndef AUDIO_BACKEND_H
#define AUDIO_BACKEND_H

#include "audio_data.h"
#include "../../libs/miniaudio.h"

#if LIBSWRESAMPLE_VERSION_MAJOR <= 3
  #define LEGACY_LIBSWRSAMPLE
#endif

void audio_buffer_write(Audio_Buffer *buf, uint8_t *audio_data, int data_must_write);
void ma_dataCallback(ma_device *ma_config, void *output, const void *input, ma_uint32 frameCount);
ma_device_config init_miniaudioConfig(Audio_Info *inf);
void audio_buffer_destroy(Audio_Buffer *buf);
Audio_Buffer *audio_buffer_init(int capacity);
void audio_buffer_reset();

#endif
