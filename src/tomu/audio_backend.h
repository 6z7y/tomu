#ifndef AUDIO_BACKEND_H
#define AUDIO_BACKEND_H

#include "structs.h"
#include "../../libs/miniaudio.h"

void audio_buffer_write(Audio_Buffer *buf, uint8_t *audio_data, int data_must_write);
void ma_dataCallback(ma_device *ma_config, void *output, const void *input, ma_uint32 frameCount);
ma_device_config init_miniaudioConfig(Audio_Info *inf);
void audio_buffer_destroy(Audio_Buffer *buf);
Audio_Buffer *audio_buffer_init(int channels, int sample_rate);
void audio_buffer_reset();
void audio_ring_resize(AudioRing *r, int new_capacity);
void audio_ring_write_float(AudioRing *r, float **data, int channels, int nb_samples);

#endif
