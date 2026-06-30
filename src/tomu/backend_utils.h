#ifndef BACKEND_UTILS
#define BACKEND_UTILS

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/dict.h>
#include "../../libs/miniaudio.h"

#include "structs.h"

enum AVSampleFormat get_interleaved(enum AVSampleFormat value);
ma_format get_ma_format(enum AVSampleFormat value);
int get_audioStream();
int get_audio_info(const char *filename);
int setup_sample_fmt_resampler(Audio_Info *inf, SwrContext **swrCTX);
void setup_speed_resampler(Audio_Info *inf, AVFrame *frame, SwrContext **speed_swrCTX);
void init_playbackstatus(TomuStatus *state, uint loop, uint shuffle);
void handle_audio_seek(int *duration_time, int64_t *total_samples_played);
void get_metadata(const char *filename);
int extract_cover(const char *input, AVFormatContext *fmt_ctx);
int extract_cover_from_stream(AVFormatContext *fmt);

#endif
