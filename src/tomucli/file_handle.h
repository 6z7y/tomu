#ifndef FILE_HANDLE_H
#define FILE_HANDLE_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "CLIENT_DATA.h"
#include "../shared/shared_control.h"

void queue_free(PlaybackQueue *queue);
void path_handle(const char *path, PlaybackQueue *queue);
void extractDir(const char *path, Dir_File *dir);

#endif
