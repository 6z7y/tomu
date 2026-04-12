#ifndef SHARE_CLIENTS_H
#define SHARE_CLIENTS_H

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "share_data.h"
#include "../shared/shared_control.h"

void send_next_from_queue(int server_fd, PlaybackQueue *queue);
void handle_playback_complete(int server_fd, PlaybackQueue *queue);
void path_handle(int server_fd, const char *path, PlaybackQueue *queue);
void verr(const char *fmt, va_list ap);
void warn(const char *fmt, ...);
void die(const char *fmt, ...);
void send_path(int server_fd, const char *path);
void extractDir(const char *path, Dir_File *dir);

inline int get_sec(double value)
{ return (int)value % 60;}

inline int get_min(double value)
{ return ((int)value % 3600) / 60;}

inline int get_hour(double value)
{ return (int)value / 3600;}

#endif
