#ifndef FILE_H
#define FILE_H

#include "structs.h"

SRC_TYPE checking_src_type(const char *src);
int handle_src(const char *src);
void queue_add(const char *src, SRC_TYPE type);
void path_handle(const char *path, LIST_FILES *queue);


#endif
