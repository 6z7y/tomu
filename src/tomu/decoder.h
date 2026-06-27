#ifndef DECODER_H
#define DECODER_H

#include "DATA.h"
#include "../shared/shared_control.h"

void *run_decoder(void *arg);
void *get_audio_info_thread(void *arg);  // ADD THIS

#endif
