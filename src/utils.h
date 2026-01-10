#ifndef UTILS_H
#define UTILS_H

#include <libavformat/avformat.h>
#include <termios.h>

void cleanUP(AVFormatContext *fmtCTX, AVCodecContext *codecCTX);
void path_handle(const char *path);

int get_sec(double value);
int get_min(double value);
int get_hour(double value);

void verr(const char *fmt, va_list ap);
void warn(const char *fmt, ...);
void die(const char *fmt, ...);

// Sets term mode to raw and hides cursor, returns the original mode
struct termios init_terminal();

// Sets term mode the provided termios struct
void deinit_terminal(const struct termios* mode);

#endif
