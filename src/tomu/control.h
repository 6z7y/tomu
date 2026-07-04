#ifndef CONTROL_H
#define CONTROL_H

#include "structs.h"

void playback_toggle(TomuStatus *state);
void playback_stop(TomuStatus *state);

void seek_playback(TomuStatus *state, dbus_int64_t offset);

void playback_speed_defualt(TomuStatus *state);
void playback_speed_increase(TomuStatus *state);
void playback_speed_decrease(TomuStatus *state);

void volume_increase(TomuStatus *state);
void volume_decrease(TomuStatus *state);

void shuffle_toggle(TomuStatus *state);
void loop_toggle(TomuStatus *state);

void playback_next_audio(TomuStatus *state);
void playback_prev_audio(TomuStatus *state);

#endif
