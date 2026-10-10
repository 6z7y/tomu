#ifndef FIFO_HANDLE_H
#define FIFO_HANDLE_H

#include "structs.h"

void fifo_file(PlayBackContext *ctx, int ON) ;
void *fifo_writer_thread(void *arg);


#endif
