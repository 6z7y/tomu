#ifndef FILE_HANDLE_H
#define FILE_HANDLE_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "CLIENT_DATA.h"
#include "../shared/shared_control.h"

void send_next_from_queue(int server_fd, PlaybackQueue *queue);
void handle_playback_complete(int server_fd, PlaybackQueue *queue);
void path_handle(int server_fd, const char *path, PlaybackQueue *queue);
void send_path(int server_fd, const char *path);
void extractDir(const char *path, Dir_File *dir);

#endif
