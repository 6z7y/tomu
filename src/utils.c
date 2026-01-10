#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavcodec/codec.h>
#include <sys/stat.h>
#include <termios.h>
#include <unistd.h>

#include "control.h"
#include "utils.h"

void cleanUP(AVFormatContext *fmtCTX, AVCodecContext *codecCTX){
  if (fmtCTX ) avformat_close_input(&fmtCTX);
  if (codecCTX ) avcodec_free_context(&codecCTX);
}

void path_handle(const char *path){
	struct stat st;
	if (stat(path, &st)<0 )  goto free;

    if (S_ISDIR(st.st_mode)) shuffle(path);
    else if (S_ISREG(st.st_mode)) playback_run(path);
    else goto free;

	return;
free:
	die("FILE: %s",strerror(errno));
}


// int shinu_now(const char *msg, AVFormatContext *fmtCTX, AVCodecContext *codecCTX){
//   fprintf(stderr, "[T]: %s\n", msg);
//
//   cleanUP(fmtCTX, codecCTX);
//
//   return 1;
// }

int get_sec(double value){
  return (int)value % 60;
}

int get_min(double value){
  return ((int)value % 3600) / 60;
}

int get_hour(double value){
  return (int)value / 3600;
}

// double get_progress_status(int *current_time, int *duration_time){
//   return (j*current_time / *duration_time) * 100.0;
//
// }

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


struct termios init_terminal() {
    struct termios orig, raw;
    tcgetattr(STDIN_FILENO, &orig);
    raw = orig;
    raw.c_lflag &= ~(ICANON | ECHO);
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    printf("\033[?25l"); // hide cursor
    fflush(stdout);
    return orig;
}

void deinit_terminal(const struct termios* mode) {
    tcsetattr(STDIN_FILENO, TCSANOW, mode);
    printf("\033[?25h"); // show cursor
    fflush(stdout);
}
