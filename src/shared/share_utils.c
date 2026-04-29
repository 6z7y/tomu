#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>

#include "share_utils.h"

// check from path is exists
int is_valid_path(const char *path)
{
  return (access(path, F_OK) == 0);
}

char *format(const char *fmt, ...)
{
  // read args
  static char buf[256];
  va_list args;
  va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
  va_end(args);

  return buf;
}

// function run command
void run_command(char *cmd)
{
  FILE *p;

  if (!(p = popen(cmd, "r"))) { 
    warn("cmd:");
    return;
  }

  pclose(p);
}

/* error handle */
void verr(const char *fmt, va_list ap)
{
	vfprintf(stderr, fmt, ap);
	if (fmt[0] && fmt[strlen(fmt) - 1] == ':') {
		fputc(' ', stderr);
		perror(NULL);
	} else {
		fputc('\n', stderr);
	}
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
