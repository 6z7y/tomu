#ifndef ERRORS_H
#define ERRORS_H

#include "dbus/dbus.h"
#include <stdarg.h>

void dbus_err_handle(DBusError *err, const char *msg);
void verr(const char *fmt, va_list ap);
int warn(const char *fmt, ...);
void die(const char *fmt, ...);

#endif
