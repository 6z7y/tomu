#ifndef AUDIO_H
#define AUDIO_H

#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/dict.h>
#include "../../libs/miniaudio.h"

#include "structs.h"

enum AVSampleFormat get_interleaved(enum AVSampleFormat value);
ma_format get_ma_format(enum AVSampleFormat value);
int get_audioStream(AVFormatContext *fmtCTX);
int get_audio_info(PlayBackContext *ctx, const char *filename);
int setup_sample_fmt_resampler(PlayBackContext *ctx, Audio_Info *inf, SwrContext **swrCTX);
void setup_speed_resampler(PlayBackContext *ctx, Audio_Info *inf, AVFrame *frame, SwrContext **speed_swrCTX);
void init_playbackstatus(PlaybackStatus *state);
void get_metadata(PlayBackContext *ctx, const char *filename);
int extract_cover(PlayBackContext *ctx);

#endif
