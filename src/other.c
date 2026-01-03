#include <libavformat/avformat.h>
#include <libavcodec/codec.h>

#include "other.h"

void cleanUP(AVFormatContext *fmtCTX, AVCodecContext *codecCTX){
  if (fmtCTX ) avformat_close_input(&fmtCTX);
  if (codecCTX ) avcodec_free_context(&codecCTX);
}

int shinu_now(const char *msg, AVFormatContext *fmtCTX, AVCodecContext *codecCTX){
  fprintf(stderr, "F: %s\n", msg);

  cleanUP(fmtCTX, codecCTX);

  return 1;
}

int get_sec(double value){
  return (int)value % 60;
}

int get_min(double value){
  return ((int)value % 3600) / 60;
}

int get_hour(double value){
  return (int)value / 3600;
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
	exit(1);
}

void help(){
  printf(
    "Usage: tomu [COMMAND] [PATH]\n"
    " Commands:\n\n"

    "   loop            : loop same sound\n"
    "   shuffle-loop    : select random file audio and loop\n"
    "   version         : show version of program\n"
    "   help            : show help message\n"

    "\nkeys:\n"
    " p = pause/resume\n"
    " q = quit\n"

    "\nExample: tomu loop [FILE.mp3]\n"
  );
}
