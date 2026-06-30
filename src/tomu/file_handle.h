#ifndef FILE_H
#define FILE_H


#include "structs.h"

void queue_free(LIST_FILES *queue);
void path_handle(const char *path, LIST_FILES *queue);
void extractDir(const char *path, LIST_FILES *dir);
void queue_add(const char *path);

#endif
