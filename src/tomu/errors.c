#include <libavformat/avformat.h>
#include <libavcodec/codec.h>
#include <string.h>
#include <unistd.h>
#include <dbus/dbus.h>

#include "errors.h"

void dbus_err_handle(DBusError *err, const char *msg)
{
  if (dbus_error_is_set(err)) {
    dbus_error_free(err);
    die("tomu: %s", msg);
  }
}

/* error handle */
void verr(const char *fmt, va_list ap)
{
	vfprintf(stderr, fmt, ap);
	if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else { fputc('\n', stderr); }
}

int warn(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	verr(fmt, ap);
    va_end(ap);
  return -1;
}

void die(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
    verr(fmt, ap);
	va_end(ap);
	exit(-1);
}

