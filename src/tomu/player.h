#ifndef PLAYER_H
#define PLAYER_H

#include "../../libs/miniaudio.h"
#include "structs.h"

void playback_handle();
int playback_run(const char *src);

void playback_toggle(PlaybackStatus *state);
void playback_stop(PlaybackStatus *state);
void seek_playback(PlaybackStatus *state, dbus_int64_t offset);
void playback_speed_defualt(PlaybackStatus *state);
void playback_speed_increase(PlaybackStatus *state);
void playback_speed_decrease(PlaybackStatus *state);
void shuffle_toggle(PlaybackStatus *state);
void playback_next_audio(PlaybackStatus *state);
void playback_prev_audio(PlaybackStatus *state);

#endif
