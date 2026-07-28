#ifndef PLAYER_H
#define PLAYER_H

#include "../../libs/miniaudio.h"
#include "structs.h"

int playback_run(PlayBackContext *ctx, const char *src);

#endif
