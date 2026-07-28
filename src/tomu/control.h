#ifndef CONTROL_H
#define CONTROL_H

#include "../../libs/miniaudio.h"
#include "structs.h"

void playback_toggle(PlayBackContext *ctx);
void playback_stop(PlayBackContext *ctx);
void seek_playback(PlayBackContext *ctx, dbus_int64_t offset);
void handle_audio_seek(PlayBackContext *ctx, int *duration_time, int64_t *total_samples_played);
void playback_speed_defualt(PlayBackContext *ctx);
void playback_speed_increase(PlayBackContext *ctx);
void playback_speed_decrease(PlayBackContext *ctx);
void shuffle_toggle(PlayBackContext *ctx);
void playback_next_audio(PlayBackContext *ctx);
void playback_prev_audio(PlayBackContext *ctx);

#endif
