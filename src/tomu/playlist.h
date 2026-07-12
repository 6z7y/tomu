#ifndef PLAYLIST_H
#define PLAYLIST_H

#include "structs.h"

SRC_TYPE extract_src_type(const char *src);
int src_handle(const char *src);
void path_handle(const char *path, LIST_FILES *queue);

#endif
