#ifndef PLAYLIST_H
#define PLAYLIST_H

#include "structs.h"

SRC_TYPE extract_src_type(const char *src);
int src_handle(PlayBackContext *ctx, const char *src);
void path_handle(PlayBackContext *ctx, const char *path, LIST_FILES *queue);

#endif
