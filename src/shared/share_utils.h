#ifndef SHARED_UTILS_H
#define SHARED_UTILS_H

#include <stdarg.h>

void verr(const char *fmt, va_list ap);
void warn(const char *fmt, ...);
void die(const char *fmt, ...);

#endif
