#ifndef BACKEND_UTILS
#define BACKEND_UTILS

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/dict.h>
#include "../../libs/miniaudio.h"

#include "audio_data.h"
#include "backend.h"
#include "control.h"
#include "../shared/share_info.h"

enum AVSampleFormat get_interleaved(enum AVSampleFormat value);
ma_format get_ma_format(enum AVSampleFormat value);

int get_audio_info(const char *filename);
int get_audio_stream();
void store_information(int audioStream_index, enum AVSampleFormat output_sample_fmt);

int setup_sample_fmt_resampler(Audio_Info *inf, SwrContext **swrCTX);
void setup_speed_resampler(Audio_Info *inf, AVFrame *frame, SwrContext **speed_swrCTX);

ma_device_config init_miniaudioConfig(Audio_Info *inf);

Audio_Buffer *audio_buffer_init(int capacity);
void audio_buffer_destroy(Audio_Buffer *buf);

void init_playbackstatus(Audio_State *state, uint loop, uint shuffle);

void handle_audio_seek(int *duration_time, int64_t *total_samples_played);
void print_metadata(AVDictionary *metadata);

#endif
