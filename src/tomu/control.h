#ifndef CONTROL_H
#define CONTROL_H

#include "DATA.h"

// void handle_key(Command cmd, TomuStatus *state);

void playback_toggle(TomuStatus *state);
void playback_stop(TomuStatus *state);

void seek_forward_sec(TomuStatus *state);
void seek_forward_min(TomuStatus *state);
void seek_backward_sec(TomuStatus *state);
void seek_backward_min(TomuStatus *state);

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
