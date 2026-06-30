#ifndef ERRORS_H
#define ERRORS_H

#include <stdarg.h>

void verr(const char *fmt, va_list ap);
int warn(const char *fmt, ...);
void die(const char *fmt, ...);

#endif
