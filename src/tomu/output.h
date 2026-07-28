#ifndef OUTPUT_H
#define OUTPUT_H

#include "structs.h"
#include "../../libs/miniaudio.h"

void audio_buffer_write(Audio_Buffer *buf, uint8_t *audio_data, int data_must_write);
void ma_dataCallback(ma_device *ma_config, void *output, const void *input, ma_uint32 frameCount);
void audio_buffer_destroy(Audio_Buffer *buf);
Audio_Buffer *audio_buffer_init(int capacity);
void audio_buffer_reset(PlayBackContext *ctx);
void *miniaudio_start(void *arg);


#endif
