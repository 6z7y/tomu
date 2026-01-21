#ifndef CONTROL_H
#define CONTROL_H

#include "backend.h"

void *handle_input(void *arg);

void playback_toggle(Audio_State *state);
void playback_pause(Audio_State *state);
void playback_resume(Audio_State *state);
void playback_stop(Audio_State *state);


void seek_forward_sec(Audio_State *state);
void seek_forward_min(Audio_State *state);
void seek_backward_sec(Audio_State *state);
void seek_backward_min(Audio_State *state);

void playback_speed_increase(Audio_State *state);
void playback_speed_decrease(Audio_State *state);

void volume_increase(Audio_State *state);
void volume_decrease(Audio_State *state);

void do_shuffle();
void stopAndShuffle(Audio_State* state);
void shuffle_toggle(Audio_State *state);
void shuffle_true(Audio_State *state);
void shuffle_false(Audio_State *state);

void loop_toggle(Audio_State *state);
void loop_true(Audio_State *state);
void loop_false(Audio_State *state);

void playback_next_audio(Audio_State *state);
void playback_prev_audio(Audio_State *state);

#endif
