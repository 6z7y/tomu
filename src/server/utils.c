#include <libavformat/avformat.h>
#include <libavcodec/codec.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>

#include "utils.h"
#include "../shared/share_info.h"

// arg compare opts
static const char *help_opts[] = {"--help", "-h", NULL};       // help
static const char *ver_opts[]  = {"--version", "-v", NULL}; // version

void sig_clean(int sig)
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
    // initlize socket protocol
    struct sockaddr_un addr = { .sun_family = AF_UNIX, .sun_path = SOCKET_PATH };
    *server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (*server_fd < 0) die("Socket:");

    // bind socket to file
    if (bind(*server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) die("Bind:");

    // enter listen mode
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

static inline int match_opt(const char *arg, const char **opts)
{
  for (int i=0; opts[i]; i++)
      if (!strcmp(arg, opts[i])) return 1;

  return 0;
}

// handle command-line arguments
int args_handle(const char *option)
{
  if (!option) return 0; // no arguments given, continue normally

  // there "--arg"
  if (option[0] == '-') {

    if      (match_opt(option, help_opts)) help();

    else if (match_opt(option, ver_opts)) printf("tomu: %s\n", TOMU_VER);

    else    warn("[T]: unknown option '%s'\n\ntry '%s --help'", option, TOMU_NAME);

    return 1;
  }

  return 0;
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
