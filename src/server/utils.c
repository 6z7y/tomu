#include <libavformat/avformat.h>
#include <libavcodec/codec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "backend.h"
#include "backend_utils.h"
#include "utils.h"
#include "socket_utils.h"
#include "../shared/share_info.h"
#include "../shared/shared_control.h"


void sig_clean()
{
  cleanUP();
  socket_mode(0, &ctx.server_fd);
  exit(0);
}

void cleanUP(){
  if (ctx.fmtCTX ) avformat_close_input(&ctx.fmtCTX);
  if (ctx.codecCTX ) avcodec_free_context(&ctx.codecCTX);
}

// socket mode ( 1 = start ), ( 0 = close )
void socket_mode(int mode, int *server_fd)
{
  unlink(SOCKET_PATH); // remove old socket file

  if (mode) {
    // 1. initlize socket protocol
    struct sockaddr_un addr = { .sun_family = AF_UNIX, .sun_path = SOCKET_PATH };
    *server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (*server_fd < 0) die("Socket:");

    // 2. bind socket to file
    if (bind(*server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) die("Bind:");

    // 3. enter listen mode
    if (listen(*server_fd, MAX_CLIENT) < 0) die("Listen:");
  }

  else close(*server_fd);
}



static inline void help()
{
  printf(
      "Usage: tomu\n\n"

      " TODO: Later\n"
  );
}


// handle command-line arguments
int args_handle(char **argv)
{
  char *option = argv[1];
  if (!option) return 0; // no arguments given, continue normally

  if (option[0] == '-' && option[1] == '-') {
    if      (!strcmp(argv[1], "--version")) printf("Tomu: %s\n", SERVER_VER);
    else if (!strcmp(argv[1], "--help")) help();

    else warn("[T]: unknown option '%s', try '%s --help'", option, SERVER_NAME);
  }
  else {
    warn("[T]: unknown option '%s'", option);
  }

  return 1;
}

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

void warn(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
	verr(fmt, ap);
  va_end(ap);
}

void die(const char *fmt, ...)
{
	va_list ap;
	va_start(ap, fmt);
  verr(fmt, ap);
	va_end(ap);
	exit(-1);
}
